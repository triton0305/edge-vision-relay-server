# Raspberry Pi Edge Vision Relay Server

[Raspberry Pi Edge Vision Client](https://github.com/triton0305/raspberry-pi-edge-vision)를 독립적으로 실행·검증하기 위한 C11 TCP 테스트 서버입니다. 객체별 `vision` JSON을 검증해 SQLite `detections`에 저장하고 ACK를 반환합니다. 팀프로젝트의 기본 연동 상대는 팀원이 개발한 Server이며, 이 저장소는 Client의 통신과 저장 동작을 별도 환경에서 재현합니다.

## Key Features

- 4-byte big-endian length-prefix 및 partial read/write 처리
- `vision` 메시지 검증, SQLite 저장 및 ACK / Error ACK
- `message_id` UNIQUE 제약을 이용한 재전송 중복 저장 방지
- 소켓 송수신 Timeout, 멈춘 연결 정리, SIGINT / SIGTERM 종료

```text
Raspberry Pi Vision Client → TCP JSON → Relay Server
→ JSON validation → SQLite detections → ACK → Client
```

서버는 `0.0.0.0:5000`에서 연결을 기다립니다. Detection 1개 = `vision` 메시지 1개 = `message_id` 1개 = `detections` row 1개 = ACK 1개입니다. 동일 차량의 반복 Detection은 별도 이력이며 실제 고유 차량 대수나 통과 차량 대수를 뜻하지 않습니다.

## Message Protocol

JSON payload와 ACK는 각각 4-byte big-endian 길이를 앞에 붙여 전송합니다. Client가 보내는 메시지 예시는 다음과 같습니다.

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000059-00000001",
  "data": {
    "frame_id": 0,
    "timestamp_ms": 1790580489875,
    "class_id": 2,
    "class_name": "car",
    "confidence": 0.2803,
    "bbox": { "x": 131, "y": 328, "width": 85, "height": 70 }
  }
}
```

`timestamp_ms`는 Client의 프레임 획득 시각(Unix ms), bbox는 원본 640×480 좌표입니다. ACK 예시는 다음과 같습니다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000059-00000001",
  "status": "ok"
}
```

검증 실패 시 `status: "error"`와 `error_code`를 반환합니다. 동일 `message_id`의 재전송은 정상 ACK를 반환하되 신규 행을 만들지 않습니다. 현재 `traffic_count`는 `unsupported_type` Error ACK를 반환하며 신규 `traffic_counts` 행을 저장하지 않습니다. 이전 테스트에서 생성된 `traffic_counts` 테이블과 기존 데이터는 보존합니다.

## Client / Server Responsibility

Client는 객체별 Detection을 생성하고 TCP/ACK, Timeout, Retry 및 재연결을 담당합니다. 이 Relay Server는 원본 Detection 저장과 ACK까지만 구현합니다. 5초·1분·시간대별 Detection 건수, 차종별 건수·비율 및 confidence 조건별 조회는 저장된 `timestamp_ms`를 기준으로 서버에서 후처리할 대상입니다. 도착 지연이나 재전송이 있어도 서버 수신 시각을 집계 기준으로 사용하지 않습니다. 통계 조회·시각화·보관/삭제 정책은 현재 구현 범위 밖입니다.

## Requirements

Ubuntu / Debian 환경에서 CMake, C compiler, cJSON, SQLite3 개발 패키지가 필요합니다.

```bash
sudo apt update
sudo apt install -y build-essential cmake libcjson-dev libsqlite3-dev
```

## Build and Run

Repository 루트에서 빌드하고 실행합니다.

```bash
git clone https://github.com/triton0305/edge-vision-relay-server.git
cd edge-vision-relay-server
cmake -S . -B build
cmake --build build -j
mkdir -p data
./build/edge_relay
```

DB 경로는 실행 위치 기준 `data/edge_vision.db`입니다. 정상 실행 시 `Edge Relay listening on 0.0.0.0:5000`이 표시되며 `Ctrl+C`로 종료합니다. Raspberry Pi에서 Client를 실행할 때는 접근 가능한 서버 IP를 지정합니다.

```bash
./build/bin/edge_vision <server_ip> 5000
```

Client의 요구 환경과 빌드는 [Client README](https://github.com/triton0305/raspberry-pi-edge-vision)를 참고하세요.

## Project Structure

```text
include/              src/
├── database.h        ├── database.c
├── network.h         ├── network.c
└── protocol.h        ├── protocol.c
                     └── main.c
```

`network`는 TCP 프레이밍과 Timeout, `protocol`은 JSON 검증 및 ACK, `database`는 SQLite 초기화와 Detection 저장, `main`은 연결 처리와 종료를 담당합니다.

## Integration Verification

USB Webcam → Raspberry Pi Client → TCP → Windows/WSL Relay Server → SQLite → ACK → Raspberry Pi 실제 E2E 흐름을 확인했습니다. 최종 Vision-only 테스트(Boot ID 59)에서 vision row 407건과 신규 `traffic_count` row 0건을 확인했습니다. 별도 서버 테스트에서는 정상 `vision`의 ACK/1행 저장, 동일 `message_id` 재전송 후 1행 유지, `traffic_count`의 `unsupported_type` ACK, 잘못된 JSON의 Error ACK, 멈춘 연결 이후 다음 Client 처리 및 SIGINT 종료를 확인했습니다. 과거 `traffic_counts` 데이터 115건은 보존했습니다.

## Traffic Feature Decision

개발 중 Client의 Tracking·기준선 통과·5초 `traffic_count` 전송을 구현하고 E2E로 검증했습니다. 이후 테스트 결과와 데이터 활용 목적을 검토해, 객체별 Detection 이력을 최종 운영 데이터로 확정했습니다. 현재 Relay Server의 수신·저장 경로는 `vision`만 지원합니다.
