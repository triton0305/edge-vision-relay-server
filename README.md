# Edge Vision Relay Server

Raspberry Pi 기반 Edge Vision Client에서 전송하는 차량 탐지 및 교통량 데이터를 수신하고, JSON 메시지를 검증한 뒤 SQLite에 저장하고 ACK를 반환하는 C 기반 TCP Server입니다.

Client 프로젝트는 아래 Repository에서 확인할 수 있습니다.

- [Raspberry Pi Edge Vision](https://github.com/triton0305/raspberry-pi-edge-vision)

## 시스템 구조

```text
Raspberry Pi
Edge Vision Client
  ├─ Camera
  ├─ YOLO Detection
  ├─ Vehicle Tracking
  ├─ Traffic Counting
  └─ TCP Client
         │
         │ 4-byte Big-endian Length Prefix + JSON
         ▼
Edge Vision Relay Server
  ├─ TCP Server
  ├─ JSON Validation
  ├─ SQLite Storage
  └─ ACK Response
```

현재 Relay Server는 Raspberry Pi Client로부터 데이터를 수신하여 로컬 SQLite에 저장하고 ACK를 반환하는 역할을 담당합니다.

## 주요 기능

- C11 기반 TCP Server
- TCP Port `5000` Listen
- 4-byte Big-endian Length Prefix 기반 메시지 프레이밍
- Partial Read / Write 처리
- 최대 Payload 크기 검증
- cJSON 기반 JSON 파싱 및 메시지 검증
- SQLite 기반 데이터 저장
- `vision` 객체 탐지 데이터 저장
- `traffic_count` 교통량 데이터 저장
- ACK / Error ACK 응답
- `message_id` 기반 중복 데이터 방지
- 동일 메시지 재전송에 대한 Idempotent 처리
- Socket Receive / Send Timeout 처리
- SIGINT / SIGTERM 정상 종료

## 프로젝트 구조

```text
edge-vision-relay-server/
├── CMakeLists.txt
├── include/
│   ├── database.h
│   ├── network.h
│   └── protocol.h
├── src/
│   ├── database.c
│   ├── main.c
│   ├── network.c
│   └── protocol.c
└── data/
```

각 모듈은 다음 역할을 담당합니다.

- `main` : 서버 초기화, Client 연결 처리, 전체 실행 흐름 및 종료 관리
- `network` : TCP 송수신, Partial I/O, Length Prefix 처리, Socket Timeout
- `protocol` : JSON 파싱, 메시지 검증, ACK 생성
- `database` : SQLite 초기화 및 데이터 저장

## TCP 프로토콜

Client와 Server 간 모든 메시지는 다음 형식을 사용합니다.

```text
4-byte Big-endian Payload Length
+
JSON Payload
```

TCP를 Stream으로 처리하며 한 번의 `recv()` 또는 `send()`가 전체 메시지를 처리한다고 가정하지 않습니다.

## Vision 메시지

객체 하나를 하나의 독립 메시지로 전송합니다.

```text
Detection 1개
= message_id 1개
= JSON 1개
= DB Row 1개
= ACK 1개
```

예시:

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000049-00000733",
  "data": {
    "frame_id": 822,
    "timestamp_ms": 1790564325017,
    "class_id": 2,
    "class_name": "car",
    "confidence": 0.85,
    "bbox": {
      "x": 217,
      "y": 222,
      "width": 53,
      "height": 35
    }
  }
}
```

지원 차량 클래스:

```text
car
motorcycle
bus
truck
```

## 교통량 집계 메시지

Raspberry Pi Client는 5초 단위로 집계한 차량 통과량을 `traffic_count` 메시지로 전송하며, Relay Server는 이를 검증하고 저장합니다.

통과 차량이 없는 구간도 Client에서 각 Count를 `0`으로 설정하여 전송합니다.

```json
{
  "version": 1,
  "type": "traffic_count",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000049-00000734",
  "data": {
    "period_start_ms": 1790564320000,
    "period_end_ms": 1790564325000,
    "car_count": 3,
    "motorcycle_count": 0,
    "bus_count": 1,
    "truck_count": 0
  }
}
```

`track_id`는 Client 내부 Tracking 및 중복 Count 방지에만 사용하며 현재 전송 메시지에는 포함하지 않습니다.

## ACK

정상 처리:

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000049-00000734",
  "status": "ok"
}
```

오류 발생:

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000049-00000734",
  "status": "error",
  "error_code": "invalid_data"
}
```

ACK의 `message_id`는 수신한 원본 메시지의 `message_id`를 그대로 사용합니다.

## 메시지 검증

Server는 수신한 JSON에 대해 다음 항목을 검증합니다.

- Protocol Version
- Message Type
- `device_id`
- `message_id`
- Vision 데이터 필드
- 차량 Class
- Confidence 범위
- Bounding Box
- Traffic Count 기간
- 차종별 Count

지원하지 않는 메시지 타입이나 잘못된 데이터는 Error ACK로 응답합니다.

## SQLite 저장

기본 DB 경로:

```text
data/edge_vision.db
```

### detections

```text
id
message_id UNIQUE
device_id
frame_id
timestamp_ms
class_id
class_name
confidence
bbox_x
bbox_y
bbox_width
bbox_height
```

### traffic_counts

```text
id
message_id UNIQUE
device_id
period_start_ms
period_end_ms
car_count
motorcycle_count
bus_count
truck_count
```

## 중복 메시지 처리

Client는 ACK 유실이나 네트워크 장애가 발생하면 동일한 `message_id`와 Payload를 재전송할 수 있습니다.

Server는 `message_id`에 UNIQUE 제약을 적용하여 동일 메시지가 다시 수신되더라도 중복 Row를 생성하지 않습니다.

```text
Client 송신
→ SQLite 저장 성공
→ ACK 유실
→ Client가 동일 message_id 재전송
→ 중복 INSERT 방지
→ ACK OK 재전송
```

이를 통해 Reliable 메시지의 재전송을 Idempotent하게 처리합니다.

## 의존성

Ubuntu / Debian 기준:

```bash
sudo apt update
sudo apt install -y build-essential cmake libcjson-dev libsqlite3-dev
```

사용 라이브러리:

- SQLite3
- cJSON

## 빌드

프로젝트 루트에서:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## 실행

DB 경로가 프로젝트 루트 기준 상대경로로 설정되어 있으므로 프로젝트 루트에서 실행합니다.

```bash
./build/edge_relay
```

정상 실행 시:

```text
Edge Relay listening on 0.0.0.0:5000
```

Server는 TCP Port `5000`에서 Client 연결을 대기합니다.

## 검증

다음 기능을 실제 테스트했습니다.

- Vision Message 수신 및 ACK
- Traffic Count Message 수신 및 ACK
- 잘못된 메시지에 대한 Error ACK
- Partial TCP Read / Write
- TCP 재연결
- 동일 `message_id` 중복 처리
- SQLite 중복 Row 방지
- Socket Timeout
- SIGINT 정상 종료

기능 구현 단계에서 다음 실제 End-to-End 통신을 확인했습니다.

```text
Raspberry Pi
→ TCP
→ Edge Vision Relay Server
→ JSON Validation
→ SQLite
→ ACK
→ Raspberry Pi
```

모듈 분리 이후에는 임시 TCP Port와 임시 SQLite DB를 사용하여 동일 기능에 대한 로컬 회귀 테스트를 수행했습니다.

Clean Build 및 다음 컴파일 경고 검사도 통과했습니다.

```text
-Wall -Wextra -Werror
```

## Git 관리

다음 실행 결과물은 Repository에서 제외합니다.

```text
build/
data/*.db
data/*.db-journal
```
