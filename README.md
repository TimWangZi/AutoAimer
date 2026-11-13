# AutoAimer · Luckfox Pico RV1103

第一次阅读代码可先看 [代码阅读指南](ARCHITECTURE.md)，再打开 `src/app.cpp`。指南按实际实现说明主循环、菜单职责和双线程画布交接。

首个可运行切片使用 C++17。主机路径用带步幅填充的合成 NV12 帧，走同一套菜单、HUD、RGB565 合成和同步显示接口。屏幕、五键输入、IMU 和测距仪均通过独立接口接入；板端驱动尚未实现时由空适配器明确报告不可用。主界面中央固定准星，测距后的落点准星与飞行曲线来自轨迹模块，左下角显示圆形水平仪。确认键触发一次测距，右键打开设置，左键返回。镜头标定页预留了入口，采样、求解和持久化待实现。

GC9A01 七针圆屏的 SPI 请求频率为 40 MHz。输入居中裁为正方形后一次缩放到 240×240，UI 保持圆屏布局；透明/实色像素使用合成快路径。显示分为 30 个 8 行条带，每个条带裁掉两侧黑角，DC/RST 使用按 `gpio1` 标签定位的 GPIO 字符设备。通用 SPI/GPIO 接口位于 `src/io/`，屏幕协议由 `Gc9a01Display` 负责。接线、构建与 ADB 调试见 [SPI 显示与调试](deploy/SPI显示与调试.md)。

主线程取帧、更新 UI 并合成，`std::thread` 发送线程独占 SPI；两块 RGB565 画布交接时不复制整帧，待发送旧帧可被更新画面替代。2026-11-13 的 SPI 分层版本已通过 ADB 部署，无摄像头输入实测 **29.60 FPS**，约 **33.78 ms/帧**，与旧版基线 **29.59 FPS** 持平；CPU 占用约 **97.5%**，SPI 错误/超时均为 0。Ubuntu 两种主机配置均通过 **8/8** 测试，无摄像头及相机/RGA 两种 ARM 版本编译成功。详见 [本次上板测试报告](docs/reports/spi-separation-board-20261113.md)。

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

RV1103 应用独立构建。媒体头文件和库使用 SDK 中与 uClibc 工具链匹配的版本：

```sh
cmake -S . -B build-rv1103 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rv1103.cmake \
  -DLUCKFOX_SDK_PATH="$HOME/luckyfox-rv1103-pico/luckfox-pico" \
  -DAUTOAIMER_BOARD=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-rv1103 -j2
```

板端路径按 SDK 的 VI 和 ISP3 示例组织：MIS5001 / RKAIQ → VI 640×480 NV12 DMA-BUF → IM2D 同步缩放和转换 → HUD 合成 → RGB565。应用先验证识别到的传感器和 IQ 文件。现有相机服务必须先释放媒体设备。待完成相机适配和上板后，可以使用 `./autoaimer --frames 60 --ppm /userdata/frame.ppm` 检查合成输出。

这份 SDK（`aaecc58d9`）的标准 Pico Buildroot 板级配置使用 MicroSD，默认板级设备树没有 MIS5001，默认 IQ 清单也没有它。MIS5001 驱动当前只有 2592×1944 的输入模式，超过 Pico 文档标称的 4 MP ISP 输入上限。VI 的 640×480 设定作用于输出，无法降低传感器原始输入。固件需要先确定相机适配方案和烧录介质。
