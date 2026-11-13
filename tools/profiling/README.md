# 显示耗时诊断

此目录的 `prepare.py` 保留的是 2026-11-12 单线程矩形发送版的插桩方案。当前正式代码已采用双线程、圆屏条带和独立 SPI/GPIO 接口，旧脚本的锚点不再适用；下面的命令用于复现当时版本，不用于本次重构验证。双线程计时需要分别记录生产与发送线程，后续适配时再处理。

`prepare.py` 将 `src/`、`cmake/` 和 CMakeLists.txt 复制到新的诊断目录，再仅对副本插入计时。正式源码和 SDK 不作修改，也不往生产主循环加入开关或测试逻辑。源代码结构变化后，插桩锚点不匹配会明确失败。

在 Ubuntu 项目目录先运行：

```sh
python3 tools/profiling/prepare.py . test-results/profile-new/source
```

使用项目原工具链构建该副本，选择 `Release`、`AUTOAIMER_BOARD=OFF`、`AUTOAIMER_SPI_DISPLAY=ON`；通过 `CMAKE_CXX_FLAGS=-DAA_PROFILE_DETAIL=0` 构建阶段计时版，改为 `1` 构建 GPIO/SPI 细分版。不要用这两个程序覆盖 `/usr/bin/autoaimer`，应分别上传到板上 `/tmp/`。

暂停正式应用后，运行诊断程序并带上 `--spi-hz 40000000 --frames 630`：前 30 帧预热，后 600 帧计时，结束后统一输出统计。两组完成后用 `/etc/init.d/S98autoaimer start` 恢复正式应用。应同时记录 SPI 错误计数、CPU governor/频率和正式程序帧率，区分插桩开销与真实运行成本。

每帧统计采用单调墙钟，包含调度等待；`process_cpu` 单列进程 CPU 时间。细粒度结果是包含测量开销的估计，不应与粗粒度阶段耗时直接相加。正式单元测试继续放在 `tests/`。

2026-11-12 实机结果见 `test-results/profile-20261112/REPORT.md`。
