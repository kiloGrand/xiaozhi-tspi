# 1. 基础配置
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 2. 绝对路径（精准匹配你的环境）
set(TOOLCHAIN_ROOT "/home/grand/tspi_projects/buildroot/output/rockchip_rk3566/host")
set(SYSROOT_PATH "${TOOLCHAIN_ROOT}/aarch64-buildroot-linux-gnu/sysroot")
set(CXX_STD_INC_PATH "${TOOLCHAIN_ROOT}/aarch64-buildroot-linux-gnu/include/c++/10.3.0")
set(CROSS_COMPILE "${TOOLCHAIN_ROOT}/bin/aarch64-buildroot-linux-gnu-")

# 3. 交叉编译器（绝对路径）
set(CMAKE_C_COMPILER "${CROSS_COMPILE}gcc")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}g++")

# 4. 关键：只设置CMAKE_SYSROOT，让CMake自动处理--sysroot，避免重复
set(CMAKE_SYSROOT "${SYSROOT_PATH}")

# 5. 编译器参数（核心：把sysroot的include放在最前面，解决#include_next问题）
# CFLAGS：先搜sysroot的C标准库，再搜其他
set(CMAKE_C_FLAGS "-I${SYSROOT_PATH}/usr/include -march=armv8-a -mtune=cortex-a55 -O2")
# CXXFLAGS：先搜sysroot的C标准库 → 再搜C++标准库 → 避免参数重复
set(CMAKE_CXX_FLAGS "-I${SYSROOT_PATH}/usr/include -isystem ${CXX_STD_INC_PATH} -isystem ${CXX_STD_INC_PATH}/aarch64-buildroot-linux-gnu -march=armv8-a -mtune=cortex-a55 -O2 -std=gnu++17")

# 6. 强制编译器使用toolchain的配套库（解决路径解析问题）
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -B${TOOLCHAIN_ROOT}/bin")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -B${TOOLCHAIN_ROOT}/bin")

# 7. 查找规则（保留）
set(CMAKE_FIND_ROOT_PATH "${SYSROOT_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 8. 禁用CMake自动添加的重复-isystem路径（关键）
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)