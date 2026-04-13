#include "xiaozhi_control.h"
#include <iostream>
#include <csignal>

// 信号处理：优雅退出程序
void signal_handler(int sig) {
    std::cout << "\n[INFO] 收到退出信号，程序即将关闭..." << std::endl;
    exit(0);
}

int main() {
    // 注册信号：Ctrl+C 优雅退出
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "========================================" << std::endl;
    std::cout << "       小智中控中心 启动成功            " << std::endl;
    std::cout << "========================================" << std::endl;

    // 启动核心中控（单例模式，永久运行）
    XiaozhiControlCenter::instance().run();

    return 0;
}