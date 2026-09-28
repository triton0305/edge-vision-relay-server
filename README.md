# Edge Vision Relay Server

[Raspberry Pi Edge Vision Client](https://github.com/triton0305/raspberry-pi-edge-vision)를 독립적으로 실행·검증하기 위한 C11 기반 TCP 테스트 서버입니다. Client가 보내는 차량 탐지 및 5초 교통량 집계 메시지를 검증해 SQLite에 저장하고 ACK를 반환합니다.

Edge Vision 팀프로젝트의 기본 연동 구조는 **Vision Client → 팀원이 개발한 Server**입니다. 이 Repository는 공개된 Vision Client의 TCP 통신과 데이터 저장 동작을 별도 환경에서도 재현할 수 있도록 제공하는 독립 서버입니다.

## Overview

```text
Raspberry Pi Vision Client
  → 4-byte length-prefix + JSON
  → Edge Vision Relay Server
  → JSON validation
  → SQLite storage
  → ACK
```

서버는 TCP Port `5000`에서 연결을 기다립니다. `vision` 메시지는 `detections`, `traffic_count` 메시지는 `traffic_counts` 테이블에 저장합니다.

## Features

- C11 기반 TCP Server 및 partial read/write 처리
- `vision`·`traffic_count` JSON 검증
- SQLite 저장 및 ACK / Error ACK 반환
- `message_id` UNIQUE 제약을 이용한 중복 저장 방지
- 동일 메시지 재전송에 대한 idempotent 처리
- Socket 송수신 Timeout 및 SIGINT / SIGTERM 종료 처리

## Requirements

Ubuntu / Debian 환경에서 CMake, C compiler, cJSON, SQLite3 개발 패키지가 필요합니다.

```bash
sudo apt update
sudo apt install -y build-essential cmake libcjson-dev libsqlite3-dev
```

## Build and Run

프로젝트를 clone한 뒤 Repository 루트에서 실행합니다.

```bash
git clone https://github.com/triton0305/edge-vision-relay-server.git
cd edge-vision-relay-server

cmake -S . -B build
cmake --build build -j
mkdir -p data
./build/edge_relay
```

정상 실행 시 다음 메시지가 출력됩니다.

```text
Edge Relay listening on 0.0.0.0:5000
```

DB는 실행 위치를 기준으로 `data/edge_vision.db`에 생성됩니다. 따라서 서버는 **Repository 루트에서 실행**해야 합니다. 종료할 때는 `Ctrl+C`를 누릅니다.

Vision Client는 Raspberry Pi에서 서버에 접근 가능한 IP 주소를 지정해 실행합니다.

```bash
./build/bin/edge_vision <server_ip> 5000
```

Windows/WSL과 Raspberry Pi를 연결하는 경우에는 Raspberry Pi에서 해당 주소와 포트에 실제로 접근할 수 있도록 네트워크 설정을 확인해야 합니다. Client의 빌드·실행 환경은 [Vision Client README](https://github.com/triton0305/raspberry-pi-edge-vision)를 참고하세요.

## Message Protocol

Client와 Server는 JSON 앞에 4-byte big-endian payload 길이를 붙여 전송합니다. 메시지 하나를 처리한 뒤 동일한 `message_id`를 담은 ACK를 반환합니다.

| Type | Data | SQLite Table |
|---|---|---|
| `vision` | Detection 1건 | `detections` |
| `traffic_count` | 5초 구간의 차종별 통과량 | `traffic_counts` |

`vision`은 차량 한 대의 `frame_id`, `timestamp_ms`, 클래스, confidence, 원본 프레임 기준 bbox를 포함합니다. `traffic_count`의 `data`는 다음 형식입니다.

```json
{
  "period_start_ms": 1790564320000,
  "period_end_ms": 1790564325000,
  "car_count": 3,
  "motorcycle_count": 0,
  "bus_count": 1,
  "truck_count": 0
}
```

두 메시지 모두 바깥쪽에 `version`, `type`, `device_id`, `message_id`, `data` 필드를 가집니다. `track_id`는 Client 내부의 Tracking 및 중복 Count 방지에 사용하며 현재 전송 메시지에는 포함하지 않습니다.

정상 ACK 예시는 다음과 같습니다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000049-00000734",
  "status": "ok"
}
```

잘못된 데이터에는 `status: "error"`와 `error_code`가 포함된 Error ACK를 반환합니다. ACK 유실 등으로 Client가 동일한 `message_id`를 재전송해도 SQLite에는 중복 Row를 만들지 않고 정상 ACK를 다시 반환합니다.

## Project Structure

```text
include/
├── database.h
├── network.h
└── protocol.h

src/
├── database.c
├── main.c
├── network.c
└── protocol.c
```

- `main`: 서버 초기화, 연결 처리, 종료
- `network`: TCP 송수신, length-prefix, timeout
- `protocol`: JSON 검증, ACK 생성
- `database`: SQLite 초기화 및 저장

## Verification

Vision·Traffic Count 메시지의 저장과 ACK, 잘못된 메시지의 Error ACK, 재연결, 중복 `message_id` 처리 및 정상 종료를 확인했습니다. Raspberry Pi Vision Client와의 실제 통합 테스트에서는 Raspberry Pi → Windows/WSL → Relay Server → SQLite → ACK 흐름을 검증했으며, 테스트 중 `detections`에 약 970행, `traffic_counts`에 약 81행이 누적됐습니다.
