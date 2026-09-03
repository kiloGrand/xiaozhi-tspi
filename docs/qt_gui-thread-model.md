# qt_gui 线程模型

## 概述

qt_gui 是一个 Qt5 触控屏应用（480×800），通过 UDP 与 `ctrl_center_with_sound` 进程通信。应用内共 **2 个线程**，跨线程通信完全依赖 Qt 信号槽的自动排队机制，无锁、无原子变量、无队列。

---

## 一、UI 布局

MainWindow 固定 480×800，自上而下分为 4 个区域 [mainwindow.cpp:125-218](qt_gui/mainwindow.cpp#L125-L218)：

```
┌────────────────────────────────┐  ▲
│         状态栏 (50px)           │  │  m_batteryLabel  电池电量
│  背景: #2c3e50                 │  │
├────────────────────────────────┤  │
│                                │  │
│       机器人区域 (200px)        │  │  m_robotEmoji  表情 emoji
│       背景: #3498db            │  │  根据 emotion 字段切换
│       居中显示 😊              │  │
│                                │  │
├────────────────────────────────┤  │
│                                │  │
│                                │  │
│     聊天消息区域 (flex)         │  │  QListView + ChatModel
│     背景: #ecf0f1              │  │  ChatDelegate 绘制气泡
│                                │  │  蓝色=机器人 绿色=用户
│                                │  │
│                                │  │
├────────────────────────────────┤  │
│       输入区域 (80px)           │  │  m_wakeButton
│       背景: #34495e            │  ▼  根据 state 显示不同文字
└────────────────────────────────┘
```

- **状态栏**：固定显示电池百分比，数据来自 `m_currentData.battery`
- **机器人区域**：大号 emoji 居中，表情由 `emotion` 字段映射，仅在 `msgType == "llm"` 时更新
- **聊天区域**：`QListView` 绑定 `ChatModel`（`QAbstractListModel`），使用自定义 `ChatDelegate` 绘制聊天气泡（机器人蓝色靠左、用户绿色靠右），自动滚到底部
- **输入区域**：单个唤醒按钮，文字跟随设备状态变化（"待机，点击唤醒聊天" / "倾听中" / "回答中" 等），仅在 `kDeviceStateIdle` 时可点击

---

## 二、线程一览表

| 线程 | 创建位置 | 核心职责 |
|------|---------|---------|
| **UI 线程**（主线程） | `main()` → `QApplication::exec()` | Qt 事件循环、窗口渲染、信号槽分发、用户交互响应 |
| **IPC 工作线程** | `IPCWorker::start()` 中 `new QThread` [ipcworker.cpp:73](qt_gui/ipcworker.cpp#L73) | UDP 监听端口 5679、JSON 解析、通过信号将数据发往 UI 线程 |

---

## 三、线程间数据流 Pipeline

```
                  ┌───────────────────────────────────────────┐
                  │              IPC 工作线程                   │
                  │                                           │
                  │  QUdpSocket::readyRead                    │
                  │       │                                   │
                  │       ▼                                   │
                  │  onReadyRead()                            │
                  │    ├─ receiveDatagram()                   │
                  │    ├─ DataParser::parse()                 │
                  │    └─ emit dataReceived(RobotData*) ────┐ │
                  └─────────────────────────────────────────│─┘
                                                            │
                                               Qt 信号槽      │
                                          (QueuedConnection)  │
                                                            │
                  ┌─────────────────────────────────────────│─┐
                  │                UI 线程                   ▼ │
                  │                                           │
                  │  MainWindow::onDataReceived(data)         │
                  │    ├─ updateStatusBar()                   │
                  │    ├─ updateWakeButton()                  │
                  │    ├─ m_robotEmoji->setText()             │
                  │    └─ m_chatModel->addMessage()           │
                  │                                           │
                  │  用户点击唤醒按钮                          │
                  │  onWakeButtonClicked()                    │
                  │    └─ m_udpSocket->writeDatagram()  ────► UDP:5678 → ctrl_center
                  └───────────────────────────────────────────┘

  ctrl_center ──UDP:5679──► IPC 工作线程      IPC 工作线程 ──信号──► UI 线程
```

### 管线说明

**下行（ctrl_center → qt_gui → UI 更新）：**

```
ctrl_center ──UDP:5679──► IPC工作线程(QUdpSocket::readyRead)
                           → onReadyRead() → JSON解析
                           → emit dataReceived(RobotData*)
                           → [Qt QueuedConnection]
                           → UI线程(MainWindow::onDataReceived)
                           → 更新 emoji / 聊天记录 / 状态栏 / 按钮
```

**上行（UI 交互 → ctrl_center）：**

```
UI线程(按钮点击) → sendConnectCmd()
                 → m_udpSocket->writeDatagram(UDP:5678) → ctrl_center
```

- 上行全在 UI 线程内完成，`writeDatagram()` 非阻塞，不跨线程。

---

## 四、各线程详细说明

### 4.1 UI 线程（Qt 主事件循环）

| 项目 | 说明 |
|------|------|
| **入口** | `main()` → `QApplication::exec()` [main.cpp:33](qt_gui/main.cpp#L33) |
| **持有的对象** | `MainWindow`（含 `ChatModel`、`IPCWorker`、`QUdpSocket`、所有 `QWidget`） |
| **核心职责** | Qt 事件循环：处理窗口渲染、信号槽分发、用户输入 |
| **与其他线程交互** | 通过 `connect()` 接收 IPC 工作线程的 `dataReceived` 信号，Qt 自动将回调投递到 UI 线程 |

### 4.2 IPC 工作线程

| 项目 | 说明 |
|------|------|
| **入口** | `IPCWorkerPrivate::start()` 槽，由 `QThread::started` 信号触发 [ipcworker.cpp:15](qt_gui/ipcworker.cpp#L15) |
| **创建方式** | `moveToThread` 模式：`IPCWorkerPrivate` 先在 UI 线程构造，然后 `moveToThread(m_thread)` 迁移到工作线程 [ipcworker.cpp:74-77](qt_gui/ipcworker.cpp#L74-L77) |
| **UDP 接收** | 事件驱动：`QUdpSocket::readyRead` → `onReadyRead()` [ipcworker.cpp:40](qt_gui/ipcworker.cpp#L40)，无轮询、无 sleep |
| **退出** | `stopRequested` 信号 → `QUdpSocket::close()`，`m_thread->quit()` + `wait()` [ipcworker.cpp:93-101](qt_gui/ipcworker.cpp#L93-L101) |

**内部事件驱动——信号槽列表：**

`IPCWorker::start()` 中完成全部连接 [ipcworker.cpp:69-89](qt_gui/ipcworker.cpp#L69-L89)：

| 连接 | 信号 | 槽 | 线程边界 | 作用 |
|------|------|-----|---------|------|
| ① | `QThread::started` | `IPCWorkerPrivate::start()` | 同线程（工作线程） | 在线程事件循环就绪后，绑定 UDP 端口 5679，连接 `readyRead` |
| ② | `QUdpSocket::readyRead` | `IPCWorkerPrivate::onReadyRead()` | 同线程（工作线程） | 收到 UDP 数据 → 解析 JSON → emit `dataReceived` |
| ③ | `IPCWorker::stopRequested` | `IPCWorkerPrivate::stop()` | **跨线程**（UI→工作） | UI 线程通知工作线程关闭 socket |
| ④ | `QThread::finished` | `IPCWorkerPrivate::deleteLater()` | 同线程（工作线程） | 线程退出时自动析构 worker 对象 |

此外还有一组**信号传信号**——不做处理，仅转发，让外部只需知道 `IPCWorker`：

| 连接 | 信号 | 信号 | 线程边界 | 作用 |
|------|------|------|---------|------|
| ⑤ | `IPCWorkerPrivate::dataReceived` | `IPCWorker::dataReceived` | 同线程（工作线程） | 信号级联：`IPCWorker::dataReceived` 在工作线程被触发 |

> 信号传信号是直接级联，第二个信号在第一个信号的线程中立即触发。因此 ⑤ 不跨线程——`IPCWorker::dataReceived` 实际运行在工作线程。

最终跨线程跳在 `MainWindow` 构造中 [mainwindow.cpp:104](qt_gui/mainwindow.cpp#L104)，信号→**槽**触发 Qt 自动排队：

| 连接 | 信号 | 槽 | 线程边界 |
|------|------|-----|---------|
| ⑥ | `IPCWorker::dataReceived` | `MainWindow::onDataReceived()` | **跨线程**（工作→UI），Qt 自动 QueuedConnection |

完整链路：**UDP 到达** → ② `readyRead`（工作线程）→ ⑤ 信号→信号级联（工作线程）→ ⑥ 信号→槽排队（**工作→UI**）→ `onDataReceived`（UI 线程）→ 更新界面。

---

## 五、Qt 信号槽跨线程 UI 更新机制

qt_gui 的跨线程通信完全依赖 Qt 信号槽，核心原理如下：

### 4.1 对象线程亲和性（Thread Affinity）

`moveToThread` 将 `IPCWorkerPrivate`（含其上 `QUdpSocket`）迁移到工作线程后：

- `QUdpSocket::readyRead` 信号在工作线程的事件循环中触发
- `IPCWorkerPrivate::onReadyRead()` 槽在工作线程执行
- `dataReceived` 信号从工作线程发出

### 4.2 自动排队连接

`IPCWorker::start()` 中做信号转发 [ipcworker.cpp:78-83](qt_gui/ipcworker.cpp#L78-L83)：

```cpp
m_worker->moveToThread(m_thread);

connect(m_thread, &QThread::started,
        m_worker, &IPCWorkerPrivate::start);      // 同线程: 直接连接
connect(m_worker, &IPCWorkerPrivate::dataReceived,
        this, &IPCWorker::dataReceived);          // 跨线程: 自动排队
```

`MainWindow` 构造中连接最终接收端 [mainwindow.cpp:104](qt_gui/mainwindow.cpp#L104)：

```cpp
connect(m_ipcWorker, &IPCWorker::dataReceived,
        this, &MainWindow::onDataReceived);       // 跨线程: 自动排队
```

**关键点**：`IPCWorkerPrivate` 和 `IPCWorker` 在两个不同的线程。当 Qt 检测到信号发送者和接收者线程不同时，自动使用 `Qt::QueuedConnection`——将槽调用封装为事件，投递到目标线程的事件队列中。对调用方完全透明：

```
IPC工作线程                       Qt 内部                        UI 线程
     │                              │                              │
     │ emit dataReceived(data)      │                              │
     │─────────────────────────────►│                              │
     │                              │  postEvent 到 UI 事件队列     │
     │  (emit 立即返回)              │                              │
     │                              │         事件循环取出          │
     │                              │─────────────────────────────►│
     │                              │      onDataReceived(data)    │
     │                              │      更新 emoji/聊天/按钮    │
```

### 4.3 与传统 QThread 继承的对比

| 方面 | 旧方案（QThread 继承） | 新方案（moveToThread） |
|------|----------------------|----------------------|
| UDP 接收 | 轮询 `hasPendingDatagrams()` + `msleep(50)` | 事件驱动 `readyRead` 信号 |
| 线程停止 | `m_running = false` 标志 + sleep 检查延迟 | `QThread::quit()` 立即退出事件循环 |
| 对象归属 | `QUdpSocket` 在 `run()` 中手动 new/delete | `QUdpSocket(this)` — QObject 树自动管理生命周期 |
| 代码量 | 65 行 | 103 行，但职责更清晰 |
