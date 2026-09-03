# ctrl_center_with_sound 线程模型

## 概述

`ctrl_center_with_sound` 将原先独立的 `ctrl_center`（中控）和 `sound`（音频）两个进程合并为**单一进程**。音频不再通过 UDP IPC 传输，而是直接在进程内完成「录音→编码→发送」和「接收→解码→播放」全链路。进程内共有 **5 个线程**，通过有界队列、原子变量、条件变量协同工作。

---

## 一、线程一览表

| 线程 | 创建位置 | 生命周期 | 核心职责 |
|------|---------|---------|---------|
| **主线程** | `main()` | 进程启动→退出 | 初始化各组件、设备激活、WebSocket 连接，之后进入永久睡眠保活 |
| **录音线程** | `AlsaCapture::start()` [AlsaCapture.cpp:82](ctrl_center_with_sound/src/AlsaCapture.cpp#L82) | init_audio() 时创建，进程退出时 stop | 从 MIC 读取 PCM → 累积 60ms → Opus 编码 → 直接发送到 WebSocket |
| **播放线程** | `AlsaPlayback::start()` [AlsaPlayback.cpp:87](ctrl_center_with_sound/src/AlsaPlayback.cpp#L87) | init_audio() 时创建，进程退出时 stop | 从下行队列取 Opus 帧 → 解码 → 写入 ALSA 声卡播放 |
| **WebSocket ASIO 线程** | `WebSocketClient::start()` [ws_client.cpp:251](ctrl_center_with_sound/src/ws_client.cpp#L251) | run() 中 `ws_client_->start()` → 连接关闭时退出 | ASIO 事件循环：TLS 握手、消息收发、回调分发 |
| **UI UDP 接收线程** | `UdpEndpoint` 构造函数 [udp_endpoint.cpp:152](ctrl_center_with_sound/src/udp_endpoint.cpp#L152) | init_ui_udp() 时创建，进程退出时析构 | 非阻塞轮询 UDP 端口 5678，接收 qt_gui 下发的 UI 指令 |

---

## 二、线程间数据流 Pipeline

```
                          ┌─────────────────────────────┐
                          │      录音线程                 │
                          │   record_worker              │
                          │                             │
                          │  PCM累积60ms → Opus编码      │
                          └─────────────┬───────────────┘
                                        │
                                        │ ws_client_->send_binary()  (直接调用, 线程安全)
                                        │
                                        ▼
                    ┌───────────────────────────────────────────────┐
                    │            WebSocket ASIO 线程                  │
                    │          (websocketpp 事件循环)                 │
                    │                                                │
                    │   on_message():                                │
                    │     二进制 → on_audio_downloaded()             │
                    │     文本   → on_text_downloaded()              │
                    │               ├─ hello → 启动音频上行           │
                    │               ├─ tts   → 状态机控制             │
                    │               └─ mcp   → handle_mcp_request()  │
                    └────────────────────┬───────────────────────────┘
                                         │
                                         │ push + notify
                                         ▼
                              ┌──────────────────────┐
                              │  opus_downlink_queue_ │
                              │  (mutex+cv, max100)  │
                              └──────────┬───────────┘
                                         │
                                         │ dequeue + 解码
                                         ▼
                              ┌─────────────────────┐
                              │      播放线程         │
                              │    play_worker       │
                              │                     │
                              │  Opus解码 → ALSA     │
                              └─────────────────────┘


   ┌──────────────────┐         UDP:5678        ┌──────────────────┐
   │   UI UDP接收线程  │◄────────────────────────│     qt_gui       │
   │   recv_thread    │                         │   (独立进程)      │
   │                  │─────UDP:5679───────────►│                  │
   │  收到数据 → 启动WS│   send_device_state()   │                  │
   └──────────────────┘   send_stt()            └──────────────────┘
```

### 管线说明

**上行音频流（录音线程 → ASIO 线程）：**

```
录音线程 ──ws_client_->send_binary()──► ASIO线程
```

- 录音回调在录音线程中执行，直接调用 `ws_client_->send_binary()`，websocketpp 内部线程安全，无需中间队列。

**下行音频流（ASIO 线程 → 播放线程）：**

```
ASIO线程 ──push──► opus_downlink_queue_ (mutex+cv) ──pop──► 播放线程
```

- 唯一使用队列+锁的跨线程通道。ASIO 回调不能阻塞，只做轻量的 push+notify。
- 队列上限 100 帧（~6 秒音频），满时丢弃最旧帧。

**UI 通道（UI UDP 线程 ↔ qt_gui 进程）：**

```
qt_gui ──UDP:5678──► UI UDP接收线程 ──on_ui_data()──► 启动 WebSocket

                      ui_ep_->send() ──UDP:5679──► qt_gui
```

- 接收：UI UDP 线程独立轮询，收到数据即触发 WebSocket 启动。
- 发送：任意线程可调用 `ui_ep_->send()`，UDP send 无状态、线程安全。

**MCP 通道（ASIO 线程内部，不跨线程）：**

```
ASIO线程 on_message(文本) → on_text_downloaded() → handle_mcp_request()
                                                     │
                                                McpServer::ParseMessage()
                                                     │
                                                ws_client_->send_text()
```

---

## 三、各线程详细说明

### 3.1 主线程

| 项目 | 说明 |
|------|------|
| **入口** | `main()` → `XiaozhiControlCenter::run()` [xiaozhi_control.cpp:459](ctrl_center_with_sound/src/xiaozhi_control.cpp#L459) |
| **执行流程** | `init_audio()` → `init_ui_udp()` → `device_activate()`（阻塞HTTP）→ `init_websocket()` → `while(true) sleep(1s)` |
| **持有资源** | `capture_`, `player_`, `encoder_`, `decoder_`, `ui_ep_`, `ws_client_`（均为 `unique_ptr` 所有权） |
| **退出** | SIGINT/SIGTERM 信号 → `exit(0)` → 全局析构 → `~XiaozhiControlCenter()` 先停录音/播放线程 |

### 3.2 录音线程

| 项目 | 说明 |
|------|------|
| **入口** | `AlsaCapture::record_worker()` [AlsaCapture.cpp:9](ctrl_center_with_sound/src/AlsaCapture.cpp#L9) |
| **创建** | `AlsaCapture::start(callback)` 中 `std::thread(&AlsaCapture::record_worker, this)` [AlsaCapture.cpp:82](ctrl_center_with_sound/src/AlsaCapture.cpp#L82) |
| **ALSA 参数** | 16kHz, 单声道, S16_LE, 阻塞读取 |
| **回调逻辑** | 见 `init_audio()` 中 lambda [xiaozhi_control.cpp:80-111](ctrl_center_with_sound/src/xiaozhi_control.cpp#L80-L111)：累积 PCM 到 60ms(1920字节) → Opus 编码 → 检查 `audio_upload_enable_` → `ws_client_->send_binary()` |
| **停止机制** | `m_is_running = false`（原子变量），主循环检查后退出；`stop()` 中 `join()` 等待线程结束 |
| **与其他线程交互** | 直接调用 `ws_client_->send_binary()`（跨入 WebSocket ASIO 线程安全），读取原子变量 `audio_upload_enable_`（由 ASIO 线程的文本回调修改） |

### 3.3 播放线程

| 项目 | 说明 |
|------|------|
| **入口** | `AlsaPlayback::play_worker()` [AlsaPlayback.cpp:13](ctrl_center_with_sound/src/AlsaPlayback.cpp#L13) |
| **创建** | `AlsaPlayback::start(callback)` 中 `std::thread(&AlsaPlayback::play_worker, this)` [AlsaPlayback.cpp:87](ctrl_center_with_sound/src/AlsaPlayback.cpp#L87) |
| **ALSA 参数** | 24kHz, 单声道, S16_LE, 阻塞写入 |
| **回调逻辑** | 见 `init_audio()` 中 lambda [xiaozhi_control.cpp:120-165](ctrl_center_with_sound/src/xiaozhi_control.cpp#L120-L165)：先消耗 `play_buffer_` 剩余 PCM → 若空则从 `opus_downlink_queue_` 取 Opus 帧 → Opus 解码 → 返回 PCM 给 ALSA |
| **停止机制** | `m_is_running = false` + `opus_downlink_cv_.notify_all()`（唤醒可能在 condition_variable 上等待的线程），`stop()` 中 `join()` |
| **与其他线程交互** | 通过 `opus_downlink_mutex_` + `opus_downlink_cv_` 与 WebSocket ASIO 线程同步（生产者-消费者） |

### 3.4 WebSocket ASIO 线程

| 项目 | 说明 |
|------|------|
| **入口** | `WebSocketClient::run_thread()` [ws_client.cpp:228](ctrl_center_with_sound/src/ws_client.cpp#L228) |
| **创建** | `start()` 中 `std::thread(&WebSocketClient::run_thread, this).detach()` [ws_client.cpp:251](ctrl_center_with_sound/src/ws_client.cpp#L251)（detach 方式，生命周期由 `m_running` 标志管理） |
| **核心** | `m_client->run()` — websocketpp ASIO 事件循环（阻塞），处理 TLS/WSS 所有 I/O |
| **回调都在本线程** | `on_open`, `on_close`, `on_message`（二进制+文本）全部由 ASIO 事件循环触发 |
| **二进制回调** | `on_audio_downloaded()` [xiaozhi_control.cpp:216](ctrl_center_with_sound/src/xiaozhi_control.cpp#L216)：非阻塞 push 到 `opus_downlink_queue_`，满时丢弃最旧帧 |
| **文本回调** | `on_text_downloaded()` [xiaozhi_control.cpp:228](ctrl_center_with_sound/src/xiaozhi_control.cpp#L228)：JSON 解析 → hello 协议 / TTS状态机 / MCP 处理 |
| **与其他线程交互** | 生产者：写入 `opus_downlink_queue_`（通知播放线程）；修改 `audio_upload_enable_`（控制录音线程）；修改 `device_state_`（任意线程读取） |

### 3.5 UI UDP 接收线程

| 项目 | 说明 |
|------|------|
| **入口** | `UdpEndpoint::startRecvThread()` lambda [udp_endpoint.cpp:152-177](ctrl_center_with_sound/src/udp_endpoint.cpp#L152-L177) |
| **创建** | `UdpEndpoint` 构造函数中自动启动 [udp_endpoint.cpp:23](ctrl_center_with_sound/src/udp_endpoint.cpp#L23) |
| **监听端口** | `UI_PORT_UP` = 5678（qt_gui 发送端） |
| **模式** | 非阻塞 `recvfrom()` + 1ms sleep（无数据时），收到数据调用 `recv_callback_` |
| **回调** | `on_ui_data()` [xiaozhi_control.cpp:207](ctrl_center_with_sound/src/xiaozhi_control.cpp#L207)：设置状态 → 启动 WebSocket |
| **与其他线程交互** | 在回调中设置 `device_state_`（原子），调用 `ws_client_->start()`（创建 ASIO 线程） |

---

## 四、ASIO 线程 ↔ 播放线程 通信详解

下行音频是唯一需要跨线程队列的通道。原因：ASIO 回调不能阻塞，而播放端需要做耗时的 Opus 解码和 ALSA 写入，必须解耦。

### 4.1 数据结构定义

[xiaozhi_control.h:111-114](ctrl_center_with_sound/include/xiaozhi_control.h#L111-L114)：

```cpp
std::queue<std::vector<uint8_t>> opus_downlink_queue_;
std::mutex opus_downlink_mutex_;
std::condition_variable opus_downlink_cv_;
static constexpr size_t kMaxDownlinkQueueSize = 100;
```

一个 `std::queue` 配一把 `std::mutex` + 一个 `std::condition_variable`，上限 100 帧。用 `std::vector<uint8_t>` 存储 Opus 帧，避免手动管理内存。

### 4.2 生产者（ASIO 线程 push）

`on_audio_downloaded()` [xiaozhi_control.cpp:216-226](ctrl_center_with_sound/src/xiaozhi_control.cpp#L216-L226)：

```cpp
void XiaozhiControlCenter::on_audio_downloaded(const char* buffer, size_t size) {
    std::unique_lock<std::mutex> lock(opus_downlink_mutex_);
    if (opus_downlink_queue_.size() >= kMaxDownlinkQueueSize) {
        opus_downlink_queue_.pop();  // 丢弃最旧帧
    }
    opus_downlink_queue_.emplace(buffer, buffer + size);
    lock.unlock();
    opus_downlink_cv_.notify_one();
}
```

要点：
- 在 ASIO 线程的 `on_message` 回调中调用，**不能阻塞**。
- 满了丢最旧帧，防止内存无限增长（100 帧 ≈ 6 秒音频，足够应对短暂积压）。
- 先 unlock 再 notify：避免播放线程被唤醒后又立即阻塞在锁上（经典的 `notify_one` 优化）。

### 4.3 消费者（播放线程 pop）

播放回调 lambda [xiaozhi_control.cpp:136-151](ctrl_center_with_sound/src/xiaozhi_control.cpp#L136-L151)：

```cpp
// 从下行队列取 Opus 帧
std::vector<uint8_t> opus_buf;
{
    std::unique_lock<std::mutex> lock(opus_downlink_mutex_);
    if (opus_downlink_queue_.empty()) {
        opus_downlink_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return !opus_downlink_queue_.empty() || !player_->is_running();
        });
        if (!player_->is_running() || opus_downlink_queue_.empty()) return 0;
    }
    opus_buf = std::move(opus_downlink_queue_.front());
    opus_downlink_queue_.pop();
}

// 解码
int pcm_data_size = 0;
decoder_->decode(opus_buf.data(), (int)opus_buf.size(),
                  play_buffer_.data(), &pcm_data_size);
```

要点：
- 用 `wait_for(100ms)` 而非 `wait()`：播放线程需要定期醒来检查 `m_is_running`，否则 `stop()` 时可能永久卡在等待上。
- 退出条件双重检查：队列非空 **或** 播放线程已停止，确保析构时能正常退出。
- `std::move` 取队列元素：避免拷贝 `vector<uint8_t>` 的开销。
- 解码在锁外进行：`decoder_->decode()` 耗时较长（~几百微秒），不在临界区内执行。

### 4.4 时序图

```
  ASIO 线程                            opus_downlink_queue_                   播放线程
     │                                        │                                   │
     │ on_message(binary)                     │                                   │
     │───lock()──────────────────────────────►│                                   │
     │───push()──────────────────────────────►│                                   │
     │───unlock()────────────────────────────►│                                   │
     │───notify_one()─────────────────────────│──────────────────────────────────►│
     │                                        │                                   │
     │                                        │  wait_for(100ms) 被唤醒           │
     │                                        │◄────lock()────────────────────────│
     │                                        │────front() + pop()───────────────►│
     │                                        │────unlock()──────────────────────►│
     │                                        │                                   │──decoder_->decode()
     │                                        │                                   │──snd_pcm_writei()
     │                                        │                                   │
     │                                        │  wait_for(100ms) 再次等待         │
     │                                        │◄──────────────────────────────────│
```

---

## 五、线程同步机制汇总

| 同步对象 | 类型 | 涉及的线程 | 用途 |
|---------|------|-----------|------|
| `opus_downlink_mutex_` | `std::mutex` | ASIO 线程 ↔ 播放线程 | 保护下行音频队列的并发读写 |
| `opus_downlink_cv_` | `std::condition_variable` | ASIO 线程 → 播放线程 | 队列非空通知 / 播放线程 100ms 超时等待 |
| `opus_downlink_queue_` | `std::queue` (有界, max 100) | ASIO 线程 → 播放线程 | 生产者-消费者：ASIO 线程 push，播放线程 pop |
| `audio_upload_enable_` | `std::atomic<bool>` | ASIO 线程 → 录音线程 | TTS 播放期间禁止上行录音（ASIO 线程写，录音线程读） |
| `device_state_` | `std::atomic<DeviceState>` | 多线程写，多线程读 | 设备状态机（Idle/Listening/Speaking/...） |
| `m_is_running` (Capture) | `std::atomic<bool>` | 主线程 → 录音线程 | 停止录音线程 |
| `m_is_running` (Playback) | `std::atomic<bool>` | 主线程 → 播放线程 | 停止播放线程 |
| `running_` (UdpEndpoint) | `std::atomic<bool>` | 主线程 → UI 接收线程 | 停止 UDP 接收循环 |
| `m_is_connected` / `m_is_shaked` | `std::atomic<bool>` | ASIO 线程写，多线程读 | WebSocket 连接状态查询 |

### 关键设计决策

**为什么下行用队列，上行不用？**

- **上行**（录音→WS发送）：录音线程是自己的节奏（每 60ms 一帧），`ws_client_->send_binary()` 内部有缓冲，不会阻塞。不需要解耦。
- **下行**（WS接收→播放）：ASIO 线程不能阻塞——`on_message` 回调如果卡住，整个 WebSocket 连接会断。所以 ASIO 回调只做轻量的 push+notify，播放线程负责耗时的 Opus 解码和 ALSA 写入。队列上限 100 帧（~6秒音频），满了就丢旧帧，防止内存无限增长。

**为什么 WebSocket 线程用 detach 而不是 join？**

WebSocket ASIO 事件循环是阻塞的（`m_client->run()`），只有连接断开才会退出。主线程需要继续执行（保活循环），不能 join 等待。线程退出时通过 `m_running` 标志和析构函数中的 `m_client->stop()` 来驱动。
