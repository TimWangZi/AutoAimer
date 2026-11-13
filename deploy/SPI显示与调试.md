# 标准 Luckfox Pico RV1103：GC9A01 七针圆屏

本固件为无摄像头验证版本：开机运行 `autoaimer`，合成画面由 `camera_sim` 提供；按钮、IMU、测距仪仍为空接口，没有假装读取硬件。未测距时界面显示无有效距离，不会生成有效落点。

## 断电后接线

屏幕针序以蓝板丝印/规格书为准；下表 Pico 一列均为用户提供图片中的**物理脚号**，不是 Linux GPIO 编号。

| 七针蓝板 | Pico 物理脚 | 信号 |
|---|---:|---|
| 1 VCC | 36 | 3V3_OUT |
| 2 GND | 13 | GND |
| 3 SCL | 14 | GPIO1_C1 / SPI0_CLK_M0 |
| 4 SDA | 15 | GPIO1_C2 / SPI0_MOSI_M0 |
| 5 DC | 4 | GPIO1_C7 / Linux GPIO55 |
| 6 CS | 12 | GPIO1_C0 / SPI0_CS0_M0 |
| 7 RST | 5 | GPIO1_C6 / Linux GPIO54 |

SDA 是 SPI MOSI，不是 I²C SDA。MISO 不接。七针版背光由模块电路供电，没有单独 BL 引脚。所有控制信号使用 3.3 V；先使用 3V3_OUT 供电，与模块内部稳压器/屏幕电压范围匹配。接口必须共地，接好后再上电。

## 显示实现

- GC9A01，240×240，RGB565 高字节先发，SPI mode 0。设备树上限、程序默认值和启动脚本均设为 **40 MHz**（实际时钟由控制器分频决定）。初始化参数取自厂家 `1.28寸初始化HSD+GC9A01.txt`，保留厂家 MADCTL `0x48` 与像素格式 `0x05`。
- 输入取居中的正方形，保持比例一次缩放到 240×240，再在最终坐标上叠加 UI。例如 640×480 输入截取 `(80,0,480,480)`。CPU 路径在最终像素位置直接采样 NV12；真实相机路径使用 SDK `improcess` 一次完成裁剪、缩放和色彩转换。
- 主界面准星位于 `(120,120)`，顶部距离文字直接按圆屏字号绘制，圆形水平仪移入左下可见区域。设置、参数、标定页分别采用圆内布局。计算所用的显示内参主点同步由 `(160,120)` 移至 `(120,120)`，焦距不变。
- 显示驱动只接收 240×240 成品帧，不再缩放、复制整张画布。每块仍为 8 行，共 **30 个窗口**；启动时取每个条带全部可见行的最小包围矩形，发送时裁掉两侧黑角。每帧传输 46,720 像素，像素负载 93,440 字节，含窗口指令 **93,770 字节 / 180 次 SPI 事务**，比完整矩形版本少约 18.8%。最大单次像素传输仍为 3,840 字节。全部圆内像素恰好覆盖一次，允许条带内少量圆外黑色像素，避免恢复到精确圆边的 143 个窗口。
- 主线程取帧、读取输入、更新菜单和合成；新增一个 `std::thread` 独占同步 `Display::present`，让计算与 SPI 等待重叠。合成器持有两块固定 RGB565 画布，共 230,400 字节。`DisplayWorker` 只管理两个索引和借用视图，不复制整帧；短时间加锁交接归属，绘制和 SPI 发送都在锁外。正在发送的画布只读，未发送旧帧可以被新画面替代，发送线程无帧时通过条件变量休眠。没有 FIFO 帧队列。
- 退出时停止生产，送完最后一张已提交的完整帧并 join，随后才释放相机和画布；SPI 错误回传主线程。`--frames N` 计数的是合成帧，在“优先最新画面”策略下实际送屏帧数可能小于 N；`--ppm` 在 join 后保存最后合成帧。主线程的退出信号标志改为无锁原子变量，允许信号到达任一线程。
- 合成时 alpha=0 直接输出背景 RGB565，alpha=255 直接输出 UI RGB565（CPU 路径还会跳过背景 NV12 采样）；只有 1..254 执行原来的混合及舍入。全部 256 个 alpha 值已验证，四个菜单预览与优化前逐字节相同。
- DC/RST 改用现有内核的 GPIO 字符设备 v1 line-handle ABI，不引入库或内核驱动。一次切换只需一个 ioctl，替代 sysfs 的 seek+write。按内核标签 `gpio1` 查找设备，再请求 bank 内偏移 23（DC）/22（RST）；本板实际为 `/dev/gpiochip3`，不能假设 gpiochip 编号等于 bank 编号。接线保持原样。
- Ubuntu 预览位于 `build-spi-host/tests/ui-preview.ppm`，同目录还会生成 `ui-preview-settings.ppm`、`ui-preview-parameters.ppm`、`ui-preview-calibration.ppm`。这些预览包含测试传感器数据；正式应用仍使用空传感器接口。
- 发送失败会返回错误，主循环退出；SPI 是单向接口，写入成功不能证明面板已接好。实际颜色、方向和接线稳定性需上板观察。

## 构建与打包

在 Ubuntu 项目目录执行：

```sh
cmake -S . -B build-spi-host -DAUTOAIMER_SPI_DISPLAY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-spi-host -j2
ctest --test-dir build-spi-host --output-on-failure

cmake -S . -B build-spi-rv1103 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rv1103.cmake \
  -DLUCKFOX_SDK_PATH=/home/timwang/luckyfox-rv1103-pico/luckfox-pico \
  -DAUTOAIMER_BOARD=OFF -DAUTOAIMER_SPI_DISPLAY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-spi-rv1103 -j2
dtc -@ -I dts -O dtb -o build-spi-rv1103/gc9a01-pico-40mhz.dtbo deploy/gc9a01-pico.dts
dtc -@ -I dts -O dtb -o build-spi-rv1103/gc9a01-speed-40mhz.dtbo deploy/gc9a01-speed-40mhz.dts
```

`AUTOAIMER_BOARD` 选择真实相机/RGA，`AUTOAIMER_SPI_DISPLAY` 独立选择屏幕，因此无摄像头固件采用 OFF/ON。

`deploy/package_spi.py --base-zip <之前已验证的标准Pico整卡镜像.zip> --sdk <SDK目录> --project <项目目录> --out <新的发布目录>` 会校验基础镜像、在项目输出目录应用 `gc9a01-pico.dts`、调用 SDK 的 `mkimage` 重打包 FIT、离线写入 ext4 应用和启动脚本、检查文件系统并生成新 ZIP。SDK 只读使用；原内核及启动链保留，设备树和 rootfs 使用副本。该脚本不执行烧录。

整卡镜像文件名为 `AutoAimer-Luckfox-Pico-RV1103-GC9A01-MicroSD.img`，可直接写入目标 MicroSD；它不是 Rockchip USB 升级用的 `update.img`。2026-11-11 的固件已写卡并回读验证，记录存于对应发布目录 `FLASH-VERIFICATION.json`。2026-11-12 的圆屏/40 MHz 版本已通过 ADB 验证，无需重烧整卡；本轮没有重新生成整卡镜像。

## ADB 调试

已有 SPI/GPIO 配置的板子使用 `gc9a01-speed-40mhz.dtbo` 动态修改频率；完整的 `gc9a01-pico-40mhz.dtbo` 用于离线合并基础设备树，不能在已配置板上重复创建 pinctrl。上传运行时覆盖到 `/etc/autoaimer/gc9a01-speed-40mhz.dtbo`、应用到 `/usr/bin/autoaimer`，并将启动脚本安装为 `/etc/init.d/S98autoaimer`。脚本在启动应用前通过 configfs 激活覆盖并核对状态，使用 `setsid` 保持 ADB 断开后的运行，日志仍保留。

2026-11-12 双线程版完成单线程 → 双线程 → 单线程对照，每次预热 3 秒后测量约 30 秒。三轮平均分别为 **21.69 / 29.73 / 21.75 FPS**；双线程版三个约 10 秒区间为 **29.70 / 29.77 / 29.73 FPS**。相比前后基线均值 **21.72 FPS** 提升 **36.88%**，平均每帧由 **46.04 ms** 降至 **33.63 ms**。全部使用 30 个裁剪条带，每帧 93,770 SPI 字节，因此测量数据量一致。

三轮 SPI 错误和超时增量均为 0。CPU governor 保持 ondemand；双线程采样点均为 1104 MHz，单线程采样点为 816/1104 MHz，没有手动设置频率。进程 CPU 占用由约 **43.6%** 升至 **97.5%**，RSS 由 2384/2420 KiB 升至 2588 KiB。当前策略持续合成并替换待发送旧帧，有利于画面新鲜度，但会花费计算在最终没有送屏的帧上。本轮没有增加节拍限制或调度优先级优化。

测量根据 SPI 发送计数计算，包含无摄像头合成输入的渲染与发送时间，不代表真实摄像头性能、面板扫描速率或长期稳定性。原始记录位于 `test-results/thread-double-20261112/`，详见 `REPORT.md`。Ubuntu 全部 7 项测试和 ARM 并发测试通过，四张菜单预览保持一致；相机/RGA 版本编译通过。TSan 检查采用测试进程兼容处理后通过，具体原因和日志见报告。

双线程版已安装为 `/usr/bin/autoaimer` 并由原启动脚本持续运行，应用线程数为 2。单线程版与启动脚本备份在 `/userdata/autoaimer-before-thread-vrzlvlyu/`。安装时还验证了 `--frames 12 --ppm` 正常退出，最后一帧与 Ubuntu 输出一致。本轮只替换应用，沿用运行时 40 MHz 覆盖和启动脚本，未修改 SDK、设备树或 CPU governor，也未重启板子或重刷整卡。

插卡、接屏、USB 上电后：

```sh
adb devices
adb shell cat /proc/device-tree/model
adb shell ls -l /dev/spidev0.0
adb shell cat /tmp/autoaimer.log
adb shell pidof autoaimer
```

型号应为 `Luckfox Pico`。应用开机运行，日志在 `/tmp/autoaimer.log`，重启后清空。要手动运行并降低总线频率：

```sh
adb shell /etc/init.d/S98autoaimer stop
adb shell /usr/bin/autoaimer --spi-hz 1000000 --frames 30 --ppm /tmp/frame.ppm
adb pull /tmp/frame.ppm
adb shell /etc/init.d/S98autoaimer start
```

必须先停止后台应用再手动运行，避免两个进程同时占用 GPIO/SPI。GPIO 占用随字符设备句柄关闭由内核释放，不再使用 export/unexport。此版本未实现实体五键，菜单交互测试在 Ubuntu 单元测试内完成。

参考：用户提供的模块规格书、电路图、GC9A01 手册与初始化表；SDK `luckfox-config` 的 SPI/pinctrl 和 FIT 覆盖实现；[Linux spidev 接口](https://docs.kernel.org/spi/spidev.html)、[Luckfox 官方烧录说明](https://wiki.luckfox.com/zh/Luckfox-Pico-Plus-Mini/Flash-image/)。
