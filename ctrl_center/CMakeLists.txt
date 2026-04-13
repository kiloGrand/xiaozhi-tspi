# ==============================================================================
# 基础配置：指定CMake最低版本（兼容GTest/现代CMake依赖查找）
# 3.14+ 版本特性：支持GTest的target模式、Boost/OpenSSL现代查找接口
# ==============================================================================
cmake_minimum_required(VERSION 3.14)

# ==============================================================================
# 项目核心信息配置
# - 项目名称：control_center（控制中心，核心功能：WebSocket客户端/HTTP请求/UDP IPC/UUID生成）
# - 编程语言：C++（核心逻辑基于C++17实现）
# - 项目版本：1.0（基础功能版）
# ==============================================================================
project(control_center    
        LANGUAGES CXX     # 强制使用C++编译器，禁用C编译器
        VERSION 1.0)      # 项目初始版本号

# ==============================================================================
# C++编译标准与构建模式配置
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)           # 采用C++17标准：适配Websocketpp/Boost的现代接口
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制使用C++17，环境不支持则编译报错（避免隐式降级）     
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)      # 默认Debug模式（便于调试网络/IPC功能，发布时改为Release）
endif()
# 定义GTest编译开关
set(BUILD_GTEST_ENABLE OFF)
# 仅当Debug模式 → 启用GTest
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_GTEST_ENABLE ON)
    message(STATUS "GTest enabled")
endif()

# ==============================================================================
# 第三方依赖库查找（按功能分类，现代CMake规范）
# 查找规则：优先系统库，找不到则报错（REQUIRED），确保依赖完整性
# ==============================================================================
# 1. 查找CURL库（HTTP请求核心依赖）
#    REQUIRED：找不到curl则终止配置，避免后续链接失败
#    生成目标：CURL::libcurl（现代CMake推荐的目标式链接）
find_package(CURL REQUIRED)

# 2. 查找Boost库（系统级功能依赖，仅需system组件）
#    COMPONENTS system：匹配Makefile的-lboost_system，提供系统调用/错误处理功能
find_package(Boost REQUIRED COMPONENTS system)

# 3. 查找OpenSSL库（加密通信依赖，SSL/TLS协议）
#    生成目标：OpenSSL::SSL / OpenSSL::Crypto（对应Makefile的-lssl -lcrypto）
find_package(OpenSSL REQUIRED)

# 4. 指定Websocketpp头文件路径（无编译库，仅头文件依赖）
#    Websocketpp：WebSocket客户端核心依赖，纯头文件库，需指定include路径
set(WEBSOCKETPP_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party/websocketpp)

# 5. 编译GoogleTest框架（单元测试依赖）
#    ${PROJECT_SOURCE_DIR}：项目根目录（当前CMakeLists.txt所在路径）
#    third_party/googletest：手动下载的GTest源码目录
#    作用：自动编译gtest/gtest_main库，为测试用例提供单元测试能力
if(BUILD_GTEST_ENABLE)
        add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/googletest)
endif()

# ==============================================================================
# 构建主程序可执行文件（my_ctrl_center）
# 源文件说明：按业务模块分类，清晰对应功能
# ==============================================================================
add_executable(my_ctrl_center 
               src/main.cpp              # 程序入口（整合所有业务模块）
               src/xiaozhi_control.cpp
        #        src/websocket_client.cpp  # WebSocket客户端（核心网络通信）
               src/ws_client.cpp         # WebSocket客户端（核心网络通信）
        #        src/ipc_udp.cpp           # UDP进程间通信（IPC）
               src/udp_endpoint.cpp      # UDP进程间通信（IPC）
               src/http.cpp              # HTTP请求（基于CURL实现）
               src/uuid.cpp)             # UUID生成（唯一标识核心逻辑）

# ==============================================================================
# 指定头文件搜索路径（解决#include路径问题）
# PRIVATE：仅当前目标（my_ctrl_center）可见，不传递给依赖目标（如测试程序）
# ==============================================================================
target_include_directories(my_ctrl_center
                           PRIVATE 
                           ${PROJECT_SOURCE_DIR}/include  # 项目自有头文件（uuid.h等）
                           ${CURL_INCLUDE_DIR}            # CURL头文件路径
                           ${Boost_INCLUDE_DIRS}          # Boost头文件路径
                           ${OPENSSL_INCLUDE_DIR}         # OpenSSL头文件路径
                           ${WEBSOCKETPP_INCLUDE_DIR})    # Websocketpp头文件路径

# ==============================================================================
# 链接依赖库（按现代CMake规范，混合兼容新旧接口）
# PRIVATE：库仅链接到my_ctrl_center，不传递给依赖目标
# ==============================================================================
target_link_libraries(my_ctrl_center
                      PRIVATE 
                      ${CURL_LIBRARIES}  # CURL库（兼容旧版本CMake的变量式链接）
                      ${Boost_LIBRARIES} # Boost system库
                      OpenSSL::SSL       # OpenSSL SSL组件（现代目标式链接）
                      OpenSSL::Crypto    # OpenSSL Crypto组件（现代目标式链接）
                      pthread            # POSIX线程库（处理网络/IPC异步逻辑）
                      )           

# ==============================================================================
# 构建测试模块
# 作用：进入tests目录执行该目录下的CMakeLists.txt，编译单元测试用例
# 生成结果：测试可执行文件run_tests（包含UUID/网络等功能的单元测试）
# ==============================================================================
if(BUILD_GTEST_ENABLE)
        add_subdirectory(tests)
endif()

# ==============================================================================
# 编译&运行说明（新手友好版）
# ==============================================================================
# 1. 创建编译目录（避免污染源码目录）
#    mkdir -p build && cd build && cmake ..
# 2. 编译所有目标（主程序+测试程序）
#    make -j$(nproc)  # -j$(nproc)：利用所有CPU核心加速编译
# 3. 仅编译测试程序
#    make run_tests
# 4. 运行主程序
#    ./my_ctrl_center
# 5. 运行指定测试用例（精准调试）
#    ./tests/run_tests --gtest_filter=UUIDConfigTest.WriteAndReadValidUuid