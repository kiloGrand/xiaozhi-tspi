
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

## qt-gui

这是基于qt的简易gui界面，用Claude开发的，如下图所示：

![GUI-1](IMG/image.png)

![GUI-2](IMG/image-1.png)
