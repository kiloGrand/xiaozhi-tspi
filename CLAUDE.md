# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

小智 AI 聊天机器人 for RK3566 (泰山派). The system runs as 3 independent processes communicating via UDP IPC on localhost. Originally from [韦东山 xiaozhi-linux](https://gitee.com/weidongshan/xiaozhi-linux), refactored to CMake + C++17 with OOP design and Google Test.

## Build Commands

### Local (Ubuntu, Debug)

```bash
# ctrl_center
cd ctrl_center && mkdir -p build && cd build && cmake .. && make -j$(nproc)
# tests:
make run_tests && ./tests/run_tests

# sound app
cd sound && mkdir -p build && cd build && cmake .. && make -j$(nproc)

# CLI gui
cd gui && mkdir -p build && cd build && cmake .. && make -j$(nproc)

# qt_gui (uses qmake, not CMake)
cd qt_gui && qmake test.pro && make
```

### Cross-compile for RK3566 (aarch64)

```bash
# Each module:
mkdir -p build_rk3566 && cd build_rk3566
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-rk3566.cmake ..
make -j$(nproc)
```

Toolchain path: `/home/grand/tspi_projects/buildroot/output/rockchip_rk3566/host`

### Run a single test

```bash
# ctrl_center tests:
./build/tests/run_tests --gtest_filter=UUIDConfigTest.WriteAndReadValidUuid

# sound tests:
./build/tests/run_tests --gtest_filter=RecordTest.RecordToFileFor5Seconds
```

## Architecture: 3-Process UDP IPC

Three processes run concurrently, communicating over localhost UDP:

| Process | Binary | Role | Ports |
|---------|--------|------|-------|
| **ctrl_center** | `my_ctrl_center` | Cloud relay + MCP server + orchestration | Audio: 5676↑/5677↓, UI: 5678↑/5679↓ |
| **sound** | `my_sound` | Full-duplex audio: record→encode→send / recv→decode→play | 5676↑/5677↓ |
| **gui** | `my_gui` or `qt_gui` | CLI or Qt-based UI display | 5678↑/5679↓ |

Data flow: `sound` records mic → Opus-encodes → UDP to `ctrl_center` → WebSocket to cloud. Cloud response → WebSocket to `ctrl_center` → UDP to `sound` → Opus-decodes → plays. UI state/emotion flows similarly on the UI ports.

## ctrl_center Module

Singleton `XiaozhiControlCenter` (see [xiaozhi_control.h](ctrl_center/include/xiaozhi_control.h)) aggregates:
- **IPC layer**: Abstract `IpcEndpoint` base → `UdpEndpoint` implementation (dual-socket send/recv, async receive thread)
- **Cloud layer**: `WebSocketClient` wrapping websocketpp for TLS WebSocket to cloud server
- **MCP Server**: Singleton `McpServer` implementing JSON-RPC 2.0 / MCP 2024-11-05 — handles `initialize`, `tools/list`, `tools/call` with tool pagination (8000-byte payload limit). Built-in tools: `self.calculator`, `self.smart_home.*`. See [mcp_server.hpp](ctrl_center/include/mcp_server.hpp).
- **HTTP**: CURL-based HTTP requests for device activation

Key dependencies: Boost (system), OpenSSL, CURL, websocketpp (header-only, in `third_party/`), nlohmann/json (bundled as `json.hpp`).

Tests in `ctrl_center/tests/` compile source files directly (not a library target).

## sound Module

Three-layer OOP architecture (see [sound/src/main.cpp](sound/src/main.cpp)):

| Layer | Classes | Role |
|-------|---------|------|
| Hardware abstraction | `AlsaAudioBase` → `AlsaCapture` / `AlsaPlayback` | RAII-managed ALSA PCM devices, threaded capture/playback with callbacks |
| Codec | `OpusEncoder` / `OpusDecoder` (via `OpusWrapper.hpp`) | PCM↔Opus conversion with Speex resampling, channel conversion |
| Transport | `UdpEndpoint` | UDP send/recv on audio ports |

Key audio params: Record at 16kHz/mono, playback at 24kHz/mono, S16_LE format, 60ms Opus frames.

Dependencies: ALSA (asound), Opus, SpeexDSP, pthread.

## qt_gui Module

Qt5 application, 480×800 fixed-size touchscreen UI. Uses qmake (`test.pro`), NOT CMake. Key classes:
- `IpcWorker` (QThread subclass) — UDP recv on port 5679, emits signals to UI thread
- `DataParser` — JSON parsing, state/emotion mapping
- `ChatModel` (QAbstractListModel) — message history with `QList<ChatMessage>`

## IPC Port Layout (hardcoded in headers)

```cpp
// Defined in xiaozhi_control.h:
AUDIO_PORT_UP    = 5676   // sound → ctrl_center
AUDIO_PORT_DOWN  = 5677   // ctrl_center → sound
UI_PORT_UP       = 5678   // gui → ctrl_center
UI_PORT_DOWN     = 5679   // ctrl_center → gui
```

## Key Conventions

- C++17, CMake 3.14+, Debug by default
- Google Test via `third_party/googletest/` (subdirectory, not FetchContent). Tests compile source `.cpp` files directly alongside test files — there are no shared library targets.
- nlohmann/json is vendored as a single header (`json.hpp`)
- OOP classes use `= delete` for copy/move where appropriate, RAII for resource management, `std::unique_ptr` / `std::atomic` over raw pointers/manual mutexes
- Error logs use Chinese/emoji in `sound/main.cpp`; other modules use English

## Agent skills

### Issue tracker

GitHub Issues on `kiloGrand/xiaozhi-tspi`. See [docs/agents/issue-tracker.md](docs/agents/issue-tracker.md).

### Triage labels

All five canonical roles use their default label strings. See [docs/agents/triage-labels.md](docs/agents/triage-labels.md).

### Domain docs

Single-context layout — one `CONTEXT.md` + `docs/adr/` at the repo root. See [docs/agents/domain.md](docs/agents/domain.md).
