# AutoAimer · Luckfox Pico RV1103

第一次阅读代码可先看 [代码阅读指南](ARCHITECTURE.md)，再打开 `src/app.cpp`。指南按实际实现说明主循环、菜单职责和双线程画布交接。

项目使用 C++17。主机路径用带步幅填充的合成 NV12 帧，走同一套菜单、HUD、RGB565 合成和显示接口。屏幕、五键输入、IMU 和测距仪均通过独立接口接入。主界面中央固定准星，测距后的落点准星与弹道轨迹来自当前预设，左下角显示圆形水平仪。主界面上下键选择 `1× / 1.5× / 2× / 3×`，确认触发一次测距，右键打开设置。弹道页提供三套可持久化预设；镜头标定页可保存完整相机灰度帧，供主机棋盘格工具求解内参与畸变。

GC9A01 七针圆屏的 SPI 请求频率为 40 MHz。输入居中裁为正方形后一次缩放到 240×240，UI 保持圆屏布局；透明/实色像素使用合成快路径。显示分为 30 个 8 行条带，每个条带裁掉两侧黑角，DC/RST 使用按 `gpio1` 标签定位的 GPIO 字符设备。通用 SPI/GPIO 接口位于 `src/io/`，屏幕协议由 `Gc9a01Display` 负责。接线、构建与 ADB 调试见 [SPI 显示与调试](deploy/SPI显示与调试.md)。

ICM-42605/ICM-42688-P 兼容驱动通过 I2C3 提供融合后的重力和倾角，SDM20 通过 UART3 提供按键触发的距离。重力参与轨迹积分，距离参与落点插值，倾角和距离同时显示在主界面。接线、设备树及诊断方法见 [传感器接入](deploy/传感器接入.md)。

主线程取帧、更新 UI 并合成，`std::thread` 发送线程独占 SPI；两块 RGB565 画布交接时不复制整帧，待发送旧帧可被更新画面替代。弹道积分会缓存，仅在预设或重力有效变化时重算。2026-11-20 的 Ubuntu 构建通过 **13/13** CTest，SC3336/RGA/SPI/传感器 ARM 版编译成功；冷启动实机 5 秒 SPI 统计约为 **25.2 FPS**，SPI 错误和超时均为 0。

历史性能对照保存在 [双线程测试报告](docs/reports/thread-double-20261112.md)和[单线程条带报告](docs/reports/band-single-20261112.md)。仓库包含源码、测试、构建及部署脚本；生成的日志、预览图、编译产物和固件包保存在本地，不随源码提交。

在 Linux 开发机执行：

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host -j2
ctest --test-dir build-host --output-on-failure
./build-host/autoaimer_sim --frames 12 --ppm preview.ppm
```

单元测试位于 `tests/`，通过 CTest 运行。五键、IMU 和测距仪的模拟适配器只存在于测试目录；`hud_preview` 测试会生成 `build-host/tests/ui-preview.ppm`。主机相机适配器负责合成帧，`--frames` 和 `--ppm` 也可用于导出无硬件预览图。

## 实际摄像头应用

标准 Pico 接入 SC3336 后，`autoaimer` 主程序通过 SDK 的 RKAIQ 和 RKMPI 取得 640×480 NV12 帧。RGA 一次裁剪、缩放并转换到 240×240，再叠加原有界面和菜单，最后通过 SPI 显示在 GC9A01 上。SC3336 驱动和 IQ 文件由板载 Buildroot 固件提供。[Luckfox 官方 CSI 文档](https://wiki.luckfox.com/zh/Luckfox-Pico-Plus-Mini/CSI-Camera/)

```sh
cmake -S . -B build-sc3336 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rv1103.cmake \
  -DLUCKFOX_SDK_PATH="$HOME/luckyfox-rv1103-pico/luckfox-pico" \
  -DAUTOAIMER_BOARD=ON -DAUTOAIMER_SPI_DISPLAY=ON \
  -DAUTOAIMER_SENSOR_HARDWARE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-sc3336 --target autoaimer autoaimer_sensor_probe -j2
```

默认裁剪输入帧中央的 480×480 区域。手动运行时用 `--crop-size 320` 可改为中央 320×320，输出尺寸固定为 240×240。自定义边长需要是正偶数，且在输入帧范围和 RGA 缩放能力内。C++ 接口是 `VideoComposer(int cropSize = 0)`，边长 `0` 表示自动裁剪。

## 棋盘格标定

以 A4 横向、实际大小 100% 打印 [棋盘格](output/pdf/camera-calibration-a4.pdf)，并用页面上的 100 mm 标尺核对尺寸。进入 `SETTINGS → LENS CALIBRATION`，从不同距离和倾角拍摄 15～25 张，按确认键保存每张原始 640×480 灰度图。样本位于 `/userdata/calibration-samples/session-N/`，完整流程见 [标定工具说明](tools/camera_calibration/README.md)。主机工具会同时输出 JSON 和板端 `camera-calibration.conf`；轨迹投影会按当前裁剪和放大窗口使用内参与 `k1/k2/p1/p2/k3`，相机图像保持原始画面。

## 弹道预设

`SETTINGS → BALLISTIC PRESETS` 提供 P1/P2/P3。上下选择，确认启用，右键编辑；编辑页包含初速、水平/竖直偏移、水平/竖直倾角、阻力系数和线性/二次阻力模型。水平以向右为正，竖直偏移以向下为正，竖直倾角以上抬为正；偏移单位为 cm。选择 `CANCEL` 可丢弃草稿。板端在修改时原子保存到 `/userdata/autoaimer/ballistic-presets.conf`，旧 v1 文件会保留原竖直值并将新增水平项初始化为 0。

板端将程序放在 `/usr/bin/autoaimer`，把 [`load_sc3336.sh`](deploy/load_sc3336.sh) 放在 `/etc/autoaimer/`，并更新 [`S98autoaimer`](deploy/overlay-autoaimer/etc/init.d/S98autoaimer)。相机版安装时创建 `/etc/autoaimer/sc3336-enabled`，传感器版创建 `/etc/autoaimer/sensors-enabled`。调整开机裁剪时，将所需边长写入 `/etc/autoaimer/sc3336-crop-size`，再执行 `/etc/init.d/S98autoaimer restart`。边长文件为空时使用默认值。无摄像头构建通过同一个服务启动。
