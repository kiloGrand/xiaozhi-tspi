# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

AI小智语音聊天机器人 - Qt5 触控屏应用 (480x800)

## 通信协议

- UDP端口: 接收5679 / 发送5678
- JSON数据格式:
  ```json
  {"state": 0, "text": "你好", "emotion": "happy", "wifi": "80", "battery": "90"}
  ```

## 架构设计

### 模块划分

1. **IPC通信层** (ipcworker.[h|cpp])
   - 继承自 QThread
   - 创建UDP socket接收数据
   - 通过信号发送数据到UI线程

2. **数据解析器** (dataparser.[h|cpp])
   - 解析JSON数据
   - 状态映射 (数字->中文状态)
   - 情绪映射 (字符串->表情)

3. **聊天记录模型** (chatmodel.[h|cpp])
   - QAbstractListModel
   - 存储消息历史 (QList<ChatMessage>)
   - 支持添加/清空消息

4. **主窗口** (mainwindow.[h|cpp|cpp])
   - 480x800 固定尺寸
   - 居中显示，无最大化

### 目录结构

```
src/
├── main.cpp              # 入口
├── mainwindow.ui         # Qt Designer
├── mainwindow.h/cpp      # 主窗口
├── ipcworker.h/cpp       # UDP通信线程
├── dataparser.h/cpp      # 数据解析
├── chatmodel.h/cpp       # 聊天记录模型
├── chatmessage.h         # 消息数据结构
└── constants.h           # 常量定义
```

## 依赖

- Qt5: core, gui, widgets, network
- nlohmann/json: JSON解析 (从原demo继承)

## 构建命令

```bash
qmake test.pro
make
```
