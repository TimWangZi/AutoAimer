# AutoAimer 项目交接

记录日期为 2026-11-20。AutoAimer 是用于 Nerf 泡沫软弹玩具的实验项目，采用 C++17 和 CMake。Git 项目位于工作区的 `Project/AutoAimer/`，当前已提交基线为 `main` 上的 `90512ea`，后续放大、标定、弹道预设、菜单整理和实体传感器改动仍在工作区。实际相机为 **SC3336**。[GitHub 仓库](https://github.com/TimWangZi/AutoAimer)

## 当前进度

- **实机画面已接通。** 标准 Luckfox Pico RV1103（MicroSD、Buildroot）通过板载 SDK 从 SC3336 获取 640×480 NV12。RGA 将居中正方形裁剪区一次缩放和转换到 240×240，再叠加原主界面或当前菜单，送到 GC9A01 圆屏。板端运行的是 `/usr/bin/autoaimer`。
- **裁剪已可配置。** 默认取中央 480×480。命令行 `--crop-size 320` 可指定边长，也可将 `320` 写入板上的 `/etc/autoaimer/sc3336-crop-size` 后重启服务。省略命令行参数或留空配置文件即可使用默认值。自定义边长应为正偶数，并满足输入尺寸和 RGA 缩放限制。
- **ICM-42605 与 SDM20 已接入。** 实机模块虽然资料名为 ICM42688，但官方身份寄存器稳定返回 `0x42`，对应 ICM-42605；兼容驱动同时接受 ICM-42688-P 的 `0x47`。IMU 通过 I2C3 输出重力与倾角，SDM20 通过 UART3 输出按确认键触发的距离；重力参与轨迹积分，距离参与落点插值，主界面显示距离、水平仪、轨迹和落点。当前安装轴为模块 `+Y` 向画面右、`-X` 向画面下、`+Z` 向镜头前方。
- **最近一次实机验证。** 最终程序完成 300 帧前台测试，退出码 0、无 OOM，峰值 RSS 7.9 MB，最低可用内存 8.8 MB，SPI 错误和超时均为 0。冷启动后显示与传感器 overlay 自动恢复，服务同时打开 ICM-42605、SDM20、SC3336 和 GC9A01；5 秒 SPI 增量约为 126 个完整帧，即约 **25.2 FPS**。探针读到重力约 `{-0.22,+9.80,-0.39} m/s²`、距离约 0.63 m。
- **主界面与弹道。** 主界面上下键切换 `1× / 1.5× / 2× / 3×`，继续复用一次裁剪和一次 RGA 缩放；投影使用实际偶数对齐窗口。P1/P2/P3 各自保存初速、水平/竖直偏移、水平/竖直倾角、阻力系数和线性/二次阻力模型，修改后持久化为 v2 配置；旧 v1 的偏移与角度自动迁移到竖直项，水平项归零。轨迹无 IMU 时使用标准重力，有有效 IMU 时替换重力，并只在输入变化时重新积分。
- **棋盘格标定。** 标定页确认键保存完整 640×480 NV12 亮度面为 PGM，按运行会话放到 `/userdata/calibration-samples/session-N/`。A4 矢量棋盘格为 10×7 个 20 mm 方格（9×6 内角点），主机 OpenCV 工具输出 JSON 和板端轻量配置；原始内参与 `k1/k2/p1/p2/k3` 只在计算轨迹投影时换算到当前裁剪和放大窗口，摄像头图像不做畸变矫正。
- **验证结果。** Ubuntu 主机构建的 13 项 CTest 全部通过；SC3336/RGA/SPI/传感器版由 SDK GCC 8.3 成功链接。传感器探针连续运行 5 次全部成功，正式服务又完成两次启停和一次冷启动验证，I2C3 与 UART3 文件描述符始终存在。五键已由用户完成接线并确认工作。

## 从哪里读代码

| 路径 | 负责内容 |
|---|---|
| [src/main.cpp](src/main.cpp)、[src/app.cpp](src/app.cpp) | 解析选项，按帧采集、更新菜单、合成并提交画面。 |
| [src/config/ballistic_store.cpp](src/config/ballistic_store.cpp)、[src/compute/](src/compute/) | 弹道预设持久化、轨迹求解和相机标定参数转换。 |
| [camera_rv1103.cpp](src/media/camera_rv1103.cpp)、[camera_sim.cpp](src/media/camera_sim.cpp) | 实机使用 RKAIQ、RKMPI VI 和 DMA-BUF，主机用合成 NV12。 |
| [src/sensors/](src/sensors/)、[deploy/传感器接入.md](deploy/传感器接入.md) | ICM-426xx 融合、SDM20 协议、Linux I2C/UART 驱动及接线说明。 |
| [video.cpp](src/media/video.cpp)、[video_geometry.h](src/media/video_geometry.h)、[video_rga.cpp](src/media/video_rga.cpp) | 计算裁剪区，处理步幅、缩放、颜色转换和 UI 合成。 |
| [src/ui/menus/](src/ui/menus/)、[tests/menu_check.cpp](tests/menu_check.cpp) | 页面实现及主机菜单导航验证。 |
| [tools/camera_calibration/](tools/camera_calibration/)、[output/pdf/camera-calibration-a4.pdf](output/pdf/camera-calibration-a4.pdf) | 棋盘格求解、预览工具和 A4 打印件。 |
| [display_worker.cpp](src/media/display_worker.cpp)、[display_gc9a01.cpp](src/media/display_gc9a01.cpp)、[src/io/](src/io/) | 双缓冲交接、圆屏条带发送、SPI 和 GPIO。 |

主线程借用相机帧直到同步合成完成。发送线程只读取成品 RGB565 画布，优先发送最新完整帧。退出时先等待发送线程，再释放 RGA 缓冲和相机。当前 SPI 请求频率为 40 MHz，圆屏每帧发送 30 个窗口、180 次 SPI 写入、93,770 字节。[代码阅读指南](ARCHITECTURE.md)

## 构建与测试

从 Git 项目根目录执行。实机版的 `AUTOAIMER_BOARD` 和 `AUTOAIMER_SPI_DISPLAY` 都为 `ON`，无摄像头板端构建使用 `OFF / ON`，主机模拟使用 `OFF / OFF`。

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host -j2
ctest --test-dir build-host --output-on-failure
./build-host/autoaimer_sim --frames 12 --crop-size 320 --ppm preview.ppm

cmake -S . -B build-sc3336 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rv1103.cmake \
  -DLUCKFOX_SDK_PATH="$HOME/luckyfox-rv1103-pico/luckfox-pico" \
  -DAUTOAIMER_BOARD=ON -DAUTOAIMER_SPI_DISPLAY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-sc3336 --target autoaimer autoaimer_sensor_probe -j2
```

实机使用 Ubuntu 上已有的 Luckfox ARM/uClibc 工具链和媒体库。固件打包脚本 `deploy/package_spi.py` 目前针对无摄像头构建，实机 SC3336 应用通过下述 ADB 流程部署。[构建参数和接线说明](README.md) · [SPI 显示与调试](deploy/SPI显示与调试.md)

## 板端部署与回退

1. 用 `adb devices -l` 确认设备，再读 `pidof autoaimer`、`/tmp/autoaimer.log` 和 `/etc/autoaimer/` 确认现状。Windows 工作区的 ADB 位于 `build-tools/adb/adb_fastboot/adb.exe`，克隆仓库后可使用本机已有的 ADB。
2. 将候选程序上传到临时目录并校验 SHA-256。先用板上现有的 `/etc/init.d/S98autoaimer stop` 停止显示服务，等待释放相机和 SPI。确认相机模块已加载，试运行时设置 `LD_LIBRARY_PATH=/oem/usr/lib:/oem/lib`，用 `--frames 60 --ppm /tmp/camera.ppm` 导出画面，再用 `adb pull` 取回。
3. 验证后部署 `/usr/bin/autoaimer`、`/etc/init.d/S98autoaimer`、`/etc/autoaimer/load_sc3336.sh` 和 `/etc/autoaimer/pico-sensors.dtbo`。相机版使用 `sc3336-enabled`，传感器版使用 `sensors-enabled`；启动脚本通过 configfs 动态启用 I2C3/UART3，无需重写 boot 分区。重启后检查进程命令行包含 `--sensors`、两个设备文件已被进程打开，并核对 SPI 计数。
4. 最近一次部署备份位于板上的 `/userdata/before-sc3336-main-2va8z9k_`，包含旧 `autoaimer`、`S98autoaimer`、`sc3336_preview` 和 `load_sc3336.sh`。需要回退时，先停止当前服务，按备份成套恢复程序和脚本。此备份对应的板端状态没有 `sc3336-enabled` 和裁剪配置文件，恢复相应状态后再启动旧服务。启动脚本备份存放在 `/userdata`，因为系统开机会执行 `/etc/init.d/` 下匹配 `S??*` 的脚本。

SPI 统计位于 `/sys/bus/spi/devices/spi0.0/statistics/`。用 `bytes_tx` 增量除以 93,770 再除以采样秒数，可估算送屏帧率。检查同一时段的 `errors` 和 `timedout`。RKAIQ 日志可能和首帧状态行交错，服务验证应结合进程、参数和实际送屏计数。

**当前板端状态：** 根分区 `mmcblk1p6` 曾在 2026-11-15 报告 EXT4/JBD2 写回错误并转为只读；按用户授权重启后恢复为读写。2026-11-20 的传感器版已永久安装到 `/usr/bin/autoaimer`，SHA-256 为 `36152b557e7e76bff45c6b214b3d4d9cc55414f18228441b1b6b043991ee6c99`。安装前程序和启动脚本备份在 `/userdata/autoaimer-before-sensors-20261120/`；未修改 boot 分区，另有只读恢复备份 `/userdata/autoaimer/boot-before-sensors-20261120.img`。若再次出现 EXT4/JBD2 错误，不要在线对已挂载根分区运行 fsck，应关机后在 Ubuntu 离线检查 SD 卡。

## 验证记录与维护约定

- 本轮传感器源码包、DTBO、探针和 ARM 程序位于 `dist/20261120-sensors/`；最终 ARM 程序 SHA-256 为 `36152b557e7e76bff45c6b214b3d4d9cc55414f18228441b1b6b043991ee6c99`。这些生成目录由 Git 忽略。
- 项目命名空间为 `autoaimer`。类和方法使用现有的中文用途注释，格式遵循 `.clang-format`。测试替身放在 `tests/`，硬件依赖由相机、显示、输入和传感器接口管理。
- 应用优先复用 SDK。确需修改 SDK 或重新烧录时，先向维护者说明原因和影响。日常上板验证通过 ADB 完成，编译产物和固件包保存在本地构建及发布目录。
