
# 小智 AI 聊天机器人【泰山派版本】

项目来源于 [韦东山老师的xiaozhi-linux](https://gitee.com/weidongshan/xiaozhi-linux)，为了适配多平台，基于 CMake 进行了重构，支持 Ubuntu 本地调试与 RK3566（泰山派）嵌入式交叉编译，同时集成 Google Test 单元测试。

# 后续计划

gui 为 简易的 CLI 界面，核心功能为通过 UDP IPC 接收 JSON 格式的 UI 数据，解析后在终端输出关键字段信息，后续工作是改成基于QT的界面。
