#ifndef CONSTANTS_H
#define CONSTANTS_H

// 屏幕尺寸
const int SCREEN_WIDTH = 470;
const int SCREEN_HEIGHT = 670;

// UDP端口
const int UI_PORT_DOWN = 5679;  // 接收数据
const int UI_PORT_UP = 5678;    // 发送数据

// 布局尺寸
const int STATUS_BAR_HEIGHT = 50;
const int ROBOT_AREA_HEIGHT = 200;
const int INPUT_AREA_HEIGHT = 80;

// 状态映射
static const int STATE_IDLE = 0;
static const int STATE_LISTENING = 1;
static const int STATE_PROCESSING = 2;
static const int STATE_SPEAKING = 3;
static const int STATE_ERROR = 4;

static const QString STATE_TEXT[] = {
    "待机中",
    "倾听中",
    "思考中",
    "回答中",
    "错误"
};

#endif // CONSTANTS_H
