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

menuconfig 的libs要勾选 libcurl 和 boost(全选)

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

解决下载源失效的问题：
```bash
cd /home/grand/tspi_projects/buildroot/dl
wget https://www.sourceware.org/pub/bzip2/bzip2-1.0.6.tar.gz
wget https://sourceforge.net/projects/boost/files/boost/1.79.0/boost_1_79_0.tar.bz2/download -O boost_1_79_0.tar.bz2
```

## 交叉编译
```bash
mkdir -p build_rk3566 && cd build_rk3566
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../toolchain-rk3566.cmake \
  -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```
编译完成后，把生成的可执行文件用 `adb push` 到开发板上面，然后`chmod +x`为文件添加可执行权限，然后运行即可。

## wpa 联网
```bash
root@RK356X:/# cat data/cfg/wpa_supplicant.conf
ctrl_interface=/var/run/wpa_supplicant
ap_scan=1
update_config=1

network={
        ssid="SSID"
        psk="PASSWORD"
        key_mgmt=WPA-PSK
}
root@RK356X:/# vi data/cfg/wpa_supplicant.conf
root@RK356X:/# wpa_supplicant -B -i wlan0 -c /data/cfg/wpa_supplicant.conf
Successfully initialized wpa_supplicant
root@RK356X:/# ping www.baidu.com
PING www.a.shifen.com (183.240.99.224) 56(84) bytes of data.
64 bytes from 183.240.99.224 (183.240.99.224): icmp_seq=1 ttl=49 time=9.38 ms
64 bytes from 183.240.99.224 (183.240.99.224): icmp_seq=2 ttl=49 time=19.5 ms
64 bytes from 183.240.99.224 (183.240.99.224): icmp_seq=3 ttl=49 time=19.6 ms
64 bytes from 183.240.99.224 (183.240.99.224): icmp_seq=4 ttl=49 time=23.3 ms
64 bytes from 183.240.99.224 (183.240.99.224): icmp_seq=5 ttl=49 time=18.3 ms
64 bytes from 183.240.99.224 (183.240.99.224): icmp_seq=6 ttl=49 time=17.0 ms
64 bytes from 183.240.99.224 (183.240.99.224): icmp_seq=7 ttl=49 time=15.1 ms
^C
```

## 时区更改

```bash
root@RK356X:/# date
Mon Mar  9 12:54:16 UTC 2026
root@RK356X:/# ls /usr/share/zoneinfo/Asia
Aden        Chongqing    Jerusalem     Novokuznetsk   Tbilisi
Almaty      Chungking    Kabul         Novosibirsk    Tehran
Amman       Colombo      Kamchatka     Omsk           Tel_Aviv
Anadyr      Dacca        Karachi       Oral           Thimbu
Aqtau       Damascus     Kashgar       Phnom_Penh     Thimphu
Aqtobe      Dhaka        Kathmandu     Pontianak      Tokyo
Ashgabat    Dili         Katmandu      Pyongyang      Tomsk
Ashkhabad   Dubai        Khandyga      Qatar          Ujung_Pandang
Atyrau      Dushanbe     Kolkata       Qyzylorda      Ulaanbaatar
Baghdad     Famagusta    Krasnoyarsk   Rangoon        Ulan_Bator
Bahrain     Gaza         Kuala_Lumpur  Riyadh         Urumqi
Baku        Harbin       Kuching       Saigon         Ust-Nera
Bangkok     Hebron       Kuwait        Sakhalin       Vientiane
Barnaul     Ho_Chi_Minh  Macao         Samarkand      Vladivostok
Beirut      Hong_Kong    Macau         Seoul          Yakutsk
Bishkek     Hovd         Magadan       Shanghai       Yangon
Brunei      Irkutsk      Makassar      Singapore      Yekaterinburg
Calcutta    Istanbul     Manila        Srednekolymsk  Yerevan
Chita       Jakarta      Muscat        Taipei
Choibalsan  Jayapura     Nicosia       Tashkent
root@RK356X:/# ls -l /etc/localtime
lrwxrwxrwx 1 root root 29 Mar 19  2025 /etc/localtime -> ../usr/share/zoneinfo/Etc/UTC
root@RK356X:/# ln -s /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
ln: failed to create symbolic link '/etc/localtime': File exists
root@RK356X:/# rm /etc/localtime
root@RK356X:/# ln -s /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
root@RK356X:/# date
Mon Mar  9 21:02:22 CST 2026
```
