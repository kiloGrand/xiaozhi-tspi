#ifndef CONSTANTS_H
#define CONSTANTS_H

// 屏幕尺寸（扣除 Weston 状态栏 32px）
const int SCREEN_WIDTH = 480;
const int SCREEN_HEIGHT = 670;

// UDP端口
const int UI_PORT_DOWN = 5679;  // 接收数据
const int UI_PORT_UP = 5678;    // 发送数据

// 布局尺寸
const int STATUS_BAR_HEIGHT = 50;
const int ROBOT_AREA_HEIGHT = 200;
const int INPUT_AREA_HEIGHT = 80;

// 设备状态枚举
typedef enum DeviceState {
    kDeviceStateUnknown = 0,
    kDeviceStateStarting = 1,
    kDeviceStateWifiConfiguring = 2,
    kDeviceStateIdle = 3,
    kDeviceStateConnecting = 4,
    kDeviceStateListening = 5,
    kDeviceStateSpeaking = 6,
    kDeviceStateUpgrading = 7,
    kDeviceStateActivating = 8,
    kDeviceStateFatalError = 9
} DeviceState;

// 状态映射
static const QString STATE_TEXT[] = {
    "未知",
    "启动中",
    "配置WiFi",
    "待机，点击唤醒聊天",
    "连接中",
    "倾听中",
    "回答中",
    "升级中",
    "激活中",
    "严重错误"
};

#endif // CONSTANTS_H
