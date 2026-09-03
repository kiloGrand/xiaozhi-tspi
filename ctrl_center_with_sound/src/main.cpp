#include "xiaozhi_control.h"
#include <iostream>
#include <csignal>

void signal_handler(int sig) {
    std::cout << "\n[INFO] 收到退出信号，程序即将关闭..." << std::endl;
    exit(0);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "========================================" << std::endl;
    std::cout << "     小智中控中心 (合并音频) 启动成功   " << std::endl;
    std::cout << "========================================" << std::endl;

    XiaozhiControlCenter::instance().run();

    return 0;
}