# ubuntu

在Ubuntu上编译运行，用于开发和debug。

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

# rk3566

在泰山派上面运行，release版本。

```bash
mkdir -p build_rk3566 && cd build_rk3566
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-rk3566.cmake
make -j$(nproc)
```

编译完成后，把生成的可执行文件用 `adb push` 到开发板上面，然后`chmod +x`为文件添加可执行权限，然后运行即可。