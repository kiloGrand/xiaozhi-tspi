
# 小智 AI 聊天机器人【泰山派版本】

项目来源于 [韦东山老师的xiaozhi-linux](https://gitee.com/weidongshan/xiaozhi-linux)，为了适配多平台，基于 CMake 进行了重构，支持 Ubuntu 本地调试与 RK3566（泰山派）嵌入式交叉编译，同时集成 Google Test 单元测试。

# 运行
先要联网，然后运行以下的命令

添加可执行权限：
```bash
chmod +x my_sound
chmod +x qt_gui
chmod +x my_control_center
```

运行：
```bash
./my_sound &
./qt_gui &
./my_control_center
```

![泰山派运行效果](IMG/ai-xiaozhi-tspi.jpg)

# 后续计划

 - [x] gui 为 简易的 CLI 界面，核心功能为通过 UDP IPC 接收 JSON 格式的 UI 数据，解析后在终端输出关键字段信息，后续工作是改成基于QT的界面。
 - [x] 基于面向对象重构 ctrl_center
 - [x] 基于面向对象重构 sound_app
 - [x] 内置 MCP 工具

## qt-gui

这是基于qt的简易gui界面，用Claude开发的，如下图所示：

![GUI-1](IMG/image.png)

![GUI-2](IMG/image-1.png)


## 基于面向对象重构 ctrl_center

### 1. 重构背景与目标
#### 1.1 原代码痛点（C 过程式）
1. **全局变量泛滥**：设备状态、通信句柄、配置参数均为全局变量，线程不安全、易引发数据竞争；
2. **C 风格回调缺陷**：函数指针无类型安全、无法绑定上下文，扩展性极差；
3. **资源手动管理**：套接字、线程需手动创建/释放，易造成内存泄漏、野指针、程序崩溃；
4. **无抽象设计**：通信模块无统一接口，新增通信方式需重构核心代码。

#### 1.2 重构核心目标
1. **模块化拆分**：按功能划分为独立模块，职责单一、解耦彻底；
2. **面向对象封装**：消除全局变量，数据与行为封装为类私有成员；
3. **抽象接口设计**：统一通信层接口，支持快速扩展通信方式；
4. **现代化改造**：替换 C 语言老旧语法，使用 C++11/14 特性实现类型安全、线程安全；
5. **自动化资源管理**：基于 RAII 机制自动释放资源，杜绝内存泄漏与崩溃。

---

### 2. 核心重构思路（模块化 + 面向对象封装）
本次重构将整个系统**按功能职责划分为 3 大核心模块**，基于**封装、继承、多态**完成面向对象设计，模块间通过标准接口交互，无直接依赖。

#### 2.1 整体模块划分
```
小智中控中心
├── 1. IPC 通信模块（跨进程通信层）
│   ├── 抽象基类：IpcEndpoint（统一通信接口）
│   └── 实现类：UdpEndpoint（UDP 协议具体实现）
├── 2. WebSocket 通信模块（云端通信层）
│   └── 实现类：WebSocketClient（TLS WebSocket 客户端封装）
└── 3. 核心业务控制模块（业务逻辑层）
    └── 实现类：XiaozhiControlCenter（单例中控核心，整合所有模块）
```

#### 2.2 各模块面向对象封装设计（功能 + 接口）
##### 模块一：IPC 通信模块（跨进程通信层）
**设计理念**：**抽象接口 + 具体实现分离**，基于多态实现统一通信标准，支持任意 IPC 协议扩展。
1. **抽象基类：IpcEndpoint**
    - **封装定位**：IPC 通信**抽象接口层**，定义所有跨进程通信的通用行为，不涉及具体协议；
    - **核心功能**：统一数据发送、接收、回调标准，约束子类实现；
    - **面向对象特性**：纯虚函数、抽象类、多态；
    - **核心接口**：
      - `send(const uint8_t* data, size_t len)`：纯虚函数，发送数据（子类必须实现）；
      - `recv(uint8_t* buffer, size_t maxlen)`：纯虚函数，接收数据（子类必须实现）；
      - `setRecvCallback(RecvCallback cb)`：设置数据接收回调。

2. **实现类：UdpEndpoint**
    - **封装定位**：IPC 通信**具体实现层**，继承 `IpcEndpoint`，专注 UDP 协议实现；
    - **核心设计**：**双 Socket 收发分离架构**（发送专用 Socket + 接收专用 Socket）；
    - **核心功能**：UDP 数据收发、后台异步接收、线程安全、自动资源释放；
    - **面向对象特性**：继承、重写、RAII、线程封装；
    - **核心接口**：
      - `UdpEndpoint(int listen_port, int send_target_port, const std::string& ip)`：构造函数（初始化端口/IP）；
      - `send(const uint8_t* data, size_t len) override`：重写发送接口；
      - `recv(uint8_t* buffer, size_t maxlen) override`：重写接收接口；
      - `~UdpEndpoint() noexcept`：析构函数（自动关闭 Socket、停止线程）。

---

##### 模块二：WebSocket 通信模块（云端通信层）
**设计理念**：**全封闭封装**，将 WebSocket 连接、TLS 认证、消息收发、回调处理全部封装为独立类，对外暴露极简接口。
1. **实现类：WebSocketClient**
    - **封装定位**：云端 TLS WebSocket 通信**独立组件**，无任何全局依赖；
    - **核心功能**：WebSocket 连接建立、消息收发（二进制/文本）、TLS 证书验证、异步回调、后台线程运行；
    - **面向对象特性**：封装、禁用拷贝、RAII、原子变量；
    - **核心接口**：
      - `WebSocketClient(...)`：构造函数（初始化服务器地址、握手消息、请求头）；
      - `set_callbacks(...)`：设置数据接收/连接关闭回调；
      - `start()`：启动客户端（后台线程运行）；
      - `send_binary/send_text`：发送数据；
      - `is_connected() const`：获取连接状态；
      - `~WebSocketClient()`：自动断开连接、释放资源。

---

##### 模块三：核心业务控制模块（业务逻辑层）
**设计理念**：**单例模式**，作为系统唯一核心中控，整合所有底层模块，处理业务逻辑、设备管理、协议解析。
1. **实现类：XiaozhiControlCenter**
    - **封装定位**：系统**业务核心调度层**，唯一对外入口，管理所有硬件、通信、业务逻辑；
    - **核心功能**：硬件信息（MAC/UUID）初始化、设备激活、UDP/WebSocket 模块管理、设备状态同步、协议解析、业务回调处理；
    - **面向对象特性**：单例模式、封装、智能指针、原子变量；
    - **核心接口**：
      - `instance()`：获取单例实例；
      - `run()`：主入口，启动整个中控系统；
    - **私有成员**：
      - 硬件信息（mac_/uuid_）、通信组件（UDP/WebSocket 智能指针）；
      - 线程安全状态变量（设备状态、音频上传开关）。

### 2.3 模块间协作关系
1. **XiaozhiControlCenter** 作为**上层业务核心**，聚合 IPC 通信模块 + WebSocket 模块；
2. 业务层不关心底层通信实现，仅通过**标准接口**调用通信模块；
3. 通信模块通过**回调函数**将数据上报给业务层处理；
4. 所有模块**无循环依赖**，可独立编译、测试、替换。

---

### 3. 应用的 C++ 高级语法/新特性（C++11/14）
重构代码全面使用现代 C++ 特性，替代老旧 C 语法，提升代码**安全性、性能、可维护性**：

| C++ 特性 | 作用 | 对应代码位置 |
|---------|------|-------------|
| **强类型枚举 `enum class`** | 避免命名冲突、类型安全，替代 C 语言 `typedef enum` | `XiaozhiControlCenter` 设备状态/监听模式 |
| **`std::atomic` 原子变量** | 多线程无锁安全访问共享变量，解决数据竞争 | 连接状态、设备状态、音频上传开关 |
| **`std::unique_ptr` 智能指针** | 独占式智能指针，自动释放动态对象，无内存泄漏 | 管理 `WebSocketClient`/`UdpEndpoint` 实例 |
| **`std::function`** | 类型安全回调，替代 C 语言函数指针，支持任意可调用对象 | IPC/WebSocket 回调定义 |
| **Lambda 表达式** | 匿名函数，便捷绑定上下文，简化异步回调编写 | UDP 接收回调、WebSocket 回调绑定 |
| **`using` 类型别名** | 替代 `typedef`，语法更简洁，支持模板 | 回调类型、WebSocket 客户端重定义 |
| **`=delete` 禁用函数** | 禁止类的拷贝/移动，杜绝非法实例操作 | 单例类、通信类禁用拷贝 |
| **纯虚函数 + 抽象基类** | 实现接口与实现分离，支持多态 | `IpcEndpoint` 抽象通信接口 |
| **RAII 资源获取即初始化** | 构造初始化资源，析构自动释放，无需手动管理 | 所有类的 Socket/线程资源管理 |
| **移动语义 `std::move`** | 减少对象拷贝，提升性能 | 回调函数赋值优化 |
| **单例模式** | 保证全局唯一实例，适配中控常驻运行特性 | `XiaozhiControlCenter` 核心类 |
| **继承与函数重写** | 复用抽象接口，实现具体协议逻辑 | `UdpEndpoint` 继承 `IpcEndpoint` |
| **访问控制（private/protected）** | 隐藏内部实现，仅暴露必要接口，实现高内聚 | 所有核心类封装 |

---

## 基于面向对象重构sound_app

### 基于alsa的应用开发

教程：
- [Linux应用开发【第八章】ALSA应用开发 - 知乎-韦东山](https://zhuanlan.zhihu.com/p/443728870)
- [小白快速入门 Linux alsa 应用编程](https://mp.weixin.qq.com/s/5TSTHWyZ8Ihul8Xv-4UclQ)

|功能|录音 (Capture)|播放 (Playback)|
|---|---|---|
|流类型|`SND_PCM_STREAM_CAPTURE`|`SND_PCM_STREAM_PLAYBACK`|
|数据读写|`snd_pcm_readi`|`snd_pcm_writei`|
|异常|`Overrun`(-EPIPE)：读太慢|`Underrun`(-EPIPE)：写太慢|

- 无文件头，纯二进制音频数据；
- 播放时**必须指定采样率 / 声道 / 格式**（代码已自动输出 ffplay 命令）；
- 例：`ffplay -f s16le -ar 16000 -ac 2 record.pcm`。

### Opus 编解码

|功能|Opus 编码 (PCM → Opus)|Opus 解码 (Opus → PCM)|
|---|---|---|
|句柄类型|`OpusEncoder*` / `SpeexResamplerState*`|`OpusDecoder*` / `SpeexResamplerState*`|
|初始化 API|`speex_resampler_init`<br><br>`opus_encoder_create`|`speex_resampler_init`<br><br>`opus_decoder_create`|
|参数配置 API|`opus_encoder_ctl` (设置比特率)|无|
|核心处理 API|`speex_resampler_process_int`<br><br>`opus_encode`|`opus_decode`<br><br>`speex_resampler_process_int`|
|资源释放 API|`speex_resampler_destroy`<br><br>`opus_encoder_destroy`|`speex_resampler_destroy`<br><br>`opus_decoder_destroy`|
|异常错误查询|`opus_strerror`|`opus_strerror`|

### 架构

采用**三层分层架构**：

|层级|核心类|职责|
|---|---|---|
|业务管理层|Main 主程序 |顶层全流程调度，实现全双工语音对讲；注册信号处理、初始化所有模块、串联**录音→编码→UDP 发送→UDP 接收→解码→播放**闭环，管理程序生命周期|
|功能模块层|AlsaCapture<br>AlsaPlayback<br>OpusEncoder<br>OpusDecoder<br>UdpEndpoint |单一职责独立模块，无相互依赖；线程化录音 / 播放、Opus 编解码 + 重采样、UDP 音频数据收发，提供标准化功能接口|
|硬件抽象层|AlsaAudioBase|封装 ALSA PCM 设备通用底层操作，为录音 / 播放子类提供统一硬件支持，实现资源 RAII 管理、参数配置、设备初始化复用|

#### 1. AlsaAudioBase（ALSA 音频基础抽象基类）

封装 ALSA PCM 设备**通用底层操作**，作为录音 / 播放子类的父类，实现代码复用；采用 RAII 智能指针管理设备资源，禁止拷贝、支持移动，避免资源冲突。

- **核心功能**：封装 ALSA PCM 设备打开、硬件参数配置、实际参数读取、资源自动释放等通用逻辑；统一处理录音 / 播放设备的初始化流程。
- **核心接口**：
    
    1. `构造函数`：初始化音频设备名、采样率、声道数、音频格式
    2. `get_sample_rate()`：获取配置的音频采样率
    3. `get_channels()`：获取配置的音频声道数
    4. `get_format()`：获取配置的音频采样格式
    5. `get_period_frames()`：获取硬件周期帧大小
    6. `get_actual_settings()`：获取硬件实际生效的音频参数
    7. `open_pcm_device()`：通用 PCM 设备初始化（录音 / 播放共用核心逻辑）
    8. `虚析构函数`：保证多态场景下子类资源正确释放

#### 2. AlsaCapture（ALSA 录音子类，继承 AlsaAudioBase）

封装 ALSA 麦克风录音全功能，**线程化采集、线程安全**，纯录音逻辑无额外处理，RAII 自动管理线程与设备资源。

- **核心功能**：实现麦克风 PCM 原始数据采集，独立线程运行；自动处理缓冲区溢出异常，通过回调函数实时输出录音数据；支持线程安全启停。
- **核心接口**：
    
    1. `继承基类构造函数`：复用音频参数初始化逻辑
    2. `start()`：启动录音线程，绑定录音数据输出回调
    3. `stop()`：停止录音线程，释放 PCM 设备与缓冲区资源
    4. `is_running()`：查询录音线程运行状态
    5. `析构函数`：自动调用 stop，确保资源无泄漏

#### 3. AlsaPlayback（ALSA 播放子类，继承 AlsaAudioBase）

封装 ALSA 扬声器播放全功能，**线程化播放、线程安全**，RAII 自动管理线程与设备资源。

- **核心功能**：实现 PCM 原始音频数据播放，独立线程运行；通过回调获取待播放数据，自动处理播放错误并恢复设备；支持线程安全启停。
- **核心接口**：
    
    1. `继承基类构造函数`：复用音频参数初始化逻辑
    2. `start()`：启动播放线程，绑定播放数据获取回调
    3. `stop()`：停止播放线程，释放 PCM 设备与缓冲区资源
    4. `is_running()`：查询播放线程运行状态
    5. `析构函数`：自动调用 stop，确保资源无泄漏

#### 4. OpusCodec（Opus 编解码整合类，包含编码器 + 解码器）

独立封装 Opus 编解码、Speex 重采样、通道转换功能，**完全解耦 ALSA 与网络模块**，RAII 管理编解码 / 重采样资源，禁止拷贝、支持移动。

- **核心功能**：实现 PCM ↔ Opus 双向格式转换；支持采样率转换、多声道 / 单声道自动转换；自动管理编码器、解码器、重采样器资源，提供健壮的异常校验。
- **核心接口**：
    1. `OpusEncoder 构造`：初始化 Opus 编码器、Speex 重采样器
    2. `OpusDecoder 构造`：初始化 Opus 解码器、Speex 重采样器
    3. `isValid()`：校验编解码器是否初始化成功
    4. `encode()`：PCM 原始数据编码为 Opus 压缩数据
    5. `decode()`：Opus 压缩数据解码为 PCM 原始数据
    6. `析构函数`：自动释放编解码、重采样所有资源

---

## MCP服务器（Model Context Protocol）架构与接口设计
基于**JSON-RPC 2.0** 实现MCP 2024-11-05标准协议，采用**单例模式+四层模块化架构**，支持工具动态注册、协议解析、工具调用、线程安全运行，适配Linux平台，预留硬件扩展接口。

### 核心依赖
- nlohmann json：JSON序列化/反序列化、协议报文解析
- C++11+：lambda表达式、`std::mutex`线程互斥、RAII资源管理

### MCP核心协议方法
遵循标准MCP协议，仅实现核心必需方法：

| 协议方法 | 功能 |
|---------|------|
| `initialize` | MCP连接初始化，协商服务端能力 |
| `tools/list` | 获取注册的MCP工具列表（支持载荷分页） |
| `tools/call` | 调用指定MCP业务工具 |

```C++
// 获取 MCP 单例服务
McpServer& server = McpServer::GetInstance();
// 注册内置工具
server.AddCommonTools();

std::string mcp_response;

// 测试1：调用工具列表
std::string req_list = R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}})";
server.ParseMessage(req_list);
mcp_response = server.GetLastResponse(); // 直接赋值，不重新定义
std::cout << "MCP执行结果: " << mcp_response << std::endl;

// 测试2：调用计算器工具
std::string req_calc = R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"self.calculator","arguments":{"a":10,"b":20, "operation":"+"}}})";
server.ParseMessage(req_calc);
mcp_response = server.GetLastResponse();
std::cout << "MCP执行结果: " << mcp_response << std::endl;
```

### 整体架构
采用**四层模块化架构**，层级解耦、单一职责、线程安全：

| 层级 | 核心类/模块 | 职责 |
|-----|-----------|------|
| 参数数据层 | `ReturnValue`/`Property`/`PropertyList` | 封装参数/返回值数据类型，提供类型安全校验、数据转换、JSON生成 |
| 工具实体层 | `McpTool` | 封装工具元数据、参数、执行回调，实现工具调用与协议序列化 |
| 业务工具层 | 内置通用工具（计算器/智能家居） | 实现具体业务逻辑，预留硬件扩展接口，标准化返回格式 |
| 协议服务层 | `McpServer` | MCP核心服务（单例），协议解析、消息路由、工具管理、响应生成 |

### 一、参数数据层（基础数据封装）
负责MCP工具的**参数定义、返回值封装、类型安全管理**，是整个架构的数据基础。

#### 1. ReturnValue（工具返回值封装类）
统一封装工具执行结果，支持布尔/整型/浮点/字符串4种标准类型，适配MCP响应格式。
- **核心功能**：多类型返回值存储、类型判断、数据获取，屏蔽底层类型差异。
- **核心接口**：

| 接口 | 功能 |
|-----|------|
| `ReturnValue(bool/int/float/string)` | 多类型构造函数 |
| `is_bool()/is_int()/is_float()/is_string()` | 判断返回值类型 |
| `get_bool()/get_int()/get_float()/get_string()` | 获取对应类型的返回值 |

#### 2. Property（单参数封装类）
封装MCP工具的单个输入参数，支持类型定义、默认值、数值范围校验。
- **核心功能**：参数元数据管理、参数合法性校验、协议JSON序列化。
- **核心接口**：

| 接口 | 功能 |
|-----|------|
| `Property(名称, 类型, [默认值/范围])` | 多场景参数构造 |
| `name()/type()` | 获取参数名称/类型 |
| `value_bool()/int()/float()/string()` | 获取参数值 |
| `to_json()` | 序列化为MCP标准参数JSON |

#### 3. PropertyList（参数列表管理类）
管理工具的所有输入参数，提供参数查找、必填参数校验、整体序列化。
- **核心功能**：参数集合管理、必填参数提取、JSON格式转换、按键值查找参数。
- **核心接口**：

| 接口 | 功能 |
|-----|------|
| `AddProperty()` | 添加工具参数 |
| `operator[]` | 按名称快速查找参数 |
| `GetRequired()` | 获取必填参数名称列表 |
| `to_json()` | 序列化为工具输入Schema JSON |

### 二、工具实体层（MCP工具封装）

封装单个MCP工具的**完整信息**，是业务逻辑与协议层的核心桥梁。
- **核心功能**：工具元数据管理、参数解析、执行回调调用、协议响应生成。
- **核心接口**：

| 接口 | 功能 |
|-----|------|
| `McpTool(名称, 描述, 参数列表, 回调)` | 构造工具实例 |
| `name()/description()/properties()` | 获取工具元数据 |
| `set_user_only()` | 设置用户专属工具权限 |
| `to_json()` | 生成工具列表标准JSON |
| `Call()` | 执行工具回调，生成MCP响应报文 |

### 三、协议服务层（核心调度引擎）

MCP服务**全局唯一入口**，采用**单例模式**，线程安全设计，实现全流程协议调度。
- **核心功能**：协议报文解析、工具注册管理、请求路由、响应生成、错误处理、工具列表分页。
- **核心接口**：

| 接口 | 功能 |
|-----|------|
| `GetInstance()` | 获取单例实例（全局唯一） |
| `AddTool/AddUserOnlyTool` | 注册普通/用户专属MCP工具 |
| `AddCommonTools()` | 注册内置通用业务工具 |
| `ParseMessage()` | 解析MCP协议报文（核心调用入口） |
| `GetLastResponse()` | 获取最后一次协议响应结果 |
| `GetToolsList()` | 生成分页工具列表（8000字节载荷限制） |
| `DoToolCall()` | 执行具体工具调用逻辑 |
| `ParseCapabilities()` | 解析客户端能力配置 |

### 四、业务工具层

内置3个标准化工具，**统一返回格式**（`success/msg/result`），预留硬件扩展接口：

| 工具名称 | 功能 | 硬件扩展预留 |
|---------|------|------------|
| `self.calculator` | 浮点数四则运算（+/-/*/） | 无 |
| `self.smart_home.get_temperature_humidity` | 查询室内温湿度 | 替换为真实传感器驱动 |
| `self.smart_home.control_light` | 灯具开关控制 | 替换为真实硬件控制接口 |

```C++
AddTool("self.smart_home.get_temperature_humidity",
    "查询当前室内环境温湿度数据，返回温度和湿度数值。\n"
    "Args:\n"
    "  无输入参数。\n"
    "Return:\n"
    "  标准化JSON响应，固定包含success、msg、result三个字段，result包含温度、湿度数据。",
    PropertyList({}),  // 无参数
    [](const PropertyList& properties) -> ReturnValue {
        // ====================== 预留硬件扩展接口 ======================
        // 后续可替换为真实传感器读取：float temp = ReadTemperature(); float humi = ReadHumidity();
        float temperature = 26.0f;  // 固定温度值
        float humidity = 30.0f;     // 固定湿度值
    
        // 统一返回格式：success + msg + result（result为JSON对象）
        return "{\"success\": true, \"msg\": \"成功获取室内温湿度\", \"result\": {\"temperature\": 26.0, \"humidity\": 30.0}}";
    });
```
