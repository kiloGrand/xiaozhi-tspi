# ubuntu

在Ubuntu上编译运行，用于开发和debug。

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

# rk3566

在泰山派上面运行，release版本。

## 前置准备

menuconfig 的libs要勾选opus和speex

```
Buildroot 配置总览
├─ 1️⃣ Target options（目标硬件）
├─ 2️⃣ Build options（构建行为）
├─ 3️⃣ Toolchain（交叉工具链）
├─ 4️⃣ System configuration（系统级设置）
├─ 5️⃣ Kernel（Linux 内核）
├─ 6️⃣ Target packages（用户空间软件包） <---- 这个里面
│   ├─ 基础工具
│   ├─ 解释器/语言
│   ├─ 网络工具
│   ├─ 图形与 UI
│   ├─ 音频/多媒体
│   ├─ 数据库
│   ├─ 文件系统
│   ├─ 开发/调试
│   └─ 自定义软件包
├─ 7️⃣ Filesystem images（根文件系统镜像格式）
├─ 8️⃣ Bootloaders（引导加载器）
├─ 9️⃣ Host utilities（宿主机工具）
└─ 🔟 Legacy config options（旧兼容项）
```

## 交叉编译

```bash
mkdir -p build_rk3566 && cd build_rk3566
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-rk3566.cmake
make -j$(nproc)
```

编译完成后，把生成的可执行文件用 `adb push` 到开发板上面，然后`chmod +x`为文件添加可执行权限，然后运行即可。
