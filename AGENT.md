# AutoAimer 项目交接

记录日期为 2026-11-13。AutoAimer 是用于 Nerf 泡沫软弹玩具的实验项目，采用 C++17 和 CMake。Git 项目位于工作区的 `Project/AutoAimer/`，本次核对的源码基线为 `main` 上的 `d1d57a4`。该提交标题误写为 SC3306，实际传感器为 **SC3336**。[GitHub 仓库](https://github.com/TimWangZi/AutoAimer)

## 当前进度

- **实机画面已接通。** 标准 Luckfox Pico RV1103（MicroSD、Buildroot）通过板载 SDK 从 SC3336 获取 640×480 NV12。RGA 将居中正方形裁剪区一次缩放和转换到 240×240，再叠加原主界面或当前菜单，送到 GC9A01 圆屏。板端运行的是 `/usr/bin/autoaimer`。
- **裁剪已可配置。** 默认取中央 480×480。命令行 `--crop-size 320` 可指定边长，也可将 `320` 写入板上的 `/etc/autoaimer/sc3336-crop-size` 后重启服务。省略命令行参数或留空配置文件即可使用默认值。自定义边长应为正偶数，并满足输入尺寸和 RGA 缩放限制。
- **最近一次实机验证。** 默认和 320×320 裁剪各完成 60 帧合成，两个配置均导出 240×240 实机画面。正式服务启动后以默认裁剪测得 **23.97 FPS**，约 10.18 秒采样内 SPI 错误和超时增量均为 **0**。现有固件已有相机模块、IQ 文件及运行库，本次通过 ADB 更新程序和启动配置完成部署。
- **界面验证与剩余硬件。** Ubuntu 的 8 项 CTest 全部通过，四个页面及圆屏预览与基线逐字节一致。`main.cpp` 仍使用 `NullButtonInput`、`NullImuSensor`、`NullDistanceSensor`，因此实体按键导航和传感器读数尚未上板验证。镜头标定页保留入口，采样、求解与保存仍待实现。

## 从哪里读代码

| 路径 | 负责内容 |
|---|---|
| [src/main.cpp](src/main.cpp)、[src/app.cpp](src/app.cpp) | 解析选项，按帧采集、更新菜单、合成并提交画面。 |
| [camera_rv1103.cpp](src/media/camera_rv1103.cpp)、[camera_sim.cpp](src/media/camera_sim.cpp) | 实机使用 RKAIQ、RKMPI VI 和 DMA-BUF，主机用合成 NV12。 |
| [video.cpp](src/media/video.cpp)、[video_geometry.h](src/media/video_geometry.h)、[video_rga.cpp](src/media/video_rga.cpp) | 计算裁剪区，处理步幅、缩放、颜色转换和 UI 合成。 |
| [src/ui/menus/](src/ui/menus/)、[tests/menu_check.cpp](tests/menu_check.cpp) | 页面实现及主机菜单导航验证。 |
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
cmake --build build-sc3336 --target autoaimer -j2
```

实机使用 Ubuntu 上已有的 Luckfox ARM/uClibc 工具链和媒体库。固件打包脚本 `deploy/package_spi.py` 目前针对无摄像头构建，实机 SC3336 应用通过下述 ADB 流程部署。[构建参数和接线说明](README.md) · [SPI 显示与调试](deploy/SPI显示与调试.md)

## 板端部署与回退

1. 用 `adb devices -l` 确认设备，再读 `pidof autoaimer`、`/tmp/autoaimer.log` 和 `/etc/autoaimer/` 确认现状。Windows 工作区的 ADB 位于 `build-tools/adb/adb_fastboot/adb.exe`，克隆仓库后可使用本机已有的 ADB。
2. 将候选程序上传到临时目录并校验 SHA-256。先用板上现有的 `/etc/init.d/S98autoaimer stop` 停止显示服务，等待释放相机和 SPI。确认相机模块已加载，试运行时设置 `LD_LIBRARY_PATH=/oem/usr/lib:/oem/lib`，用 `--frames 60 --ppm /tmp/camera.ppm` 导出画面，再用 `adb pull` 取回。
3. 验证后部署 `/usr/bin/autoaimer`、`/etc/init.d/S98autoaimer` 和 `/etc/autoaimer/load_sc3336.sh`。相机版使用 `/etc/autoaimer/sc3336-enabled` 标记启用模块加载与 OEM 运行库路径。重启服务后检查新进程的可执行文件、命令行及 SPI 计数。确认持续出帧后再记录送屏帧率。
4. 最近一次部署备份位于板上的 `/userdata/before-sc3336-main-2va8z9k_`，包含旧 `autoaimer`、`S98autoaimer`、`sc3336_preview` 和 `load_sc3336.sh`。需要回退时，先停止当前服务，按备份成套恢复程序和脚本。此备份对应的板端状态没有 `sc3336-enabled` 和裁剪配置文件，恢复相应状态后再启动旧服务。启动脚本备份存放在 `/userdata`，因为系统开机会执行 `/etc/init.d/` 下匹配 `S??*` 的脚本。

SPI 统计位于 `/sys/bus/spi/devices/spi0.0/statistics/`。用 `bytes_tx` 增量除以 93,770 再除以采样秒数，可估算送屏帧率。检查同一时段的 `errors` 和 `timedout`。RKAIQ 日志可能和首帧状态行交错，服务验证应结合进程、参数和实际送屏计数。

## 验证记录与维护约定

- 最新本地记录在 `test-results/sc3336-main-20261113/`。`installation.json` 和 `installation-verified.log` 对应成功部署，`build.log`、`smoke.json`、`default.png`、`crop320.png` 对应构建、试运行和画面。目录由 Git 忽略，克隆仓库后可按上面的步骤重新生成。
- 项目命名空间为 `autoaimer`。类和方法使用现有的中文用途注释，格式遵循 `.clang-format`。测试替身放在 `tests/`，硬件依赖由相机、显示、输入和传感器接口管理。
- 应用优先复用 SDK。确需修改 SDK 或重新烧录时，先向维护者说明原因和影响。日常上板验证通过 ADB 完成，编译产物和固件包保存在本地构建及发布目录。
