# 相机镜头畸变标定

## 打印和拍摄

1. 以 **A4 横向、实际大小（100%）** 打印[棋盘格](../../output/pdf/camera-calibration-a4.pdf)。图案是 10×7 格，OpenCV 使用其中的 9×6 个内角点。用直尺核对标尺，两条刻线相距 100 mm，或横向连续五格合计 100 mm。
2. 将整张纸平贴在硬板上，保留周围白边。使用同一相机、焦距和分辨率拍摄，保留原图。把棋盘格拍在画面中央和四周，用不同倾角与距离拍 15–25 张，每一张都让整块棋盘格进入画面。
3. 将原图放在同一目录。工具至少需要 10 张能检测到完整棋盘格的照片。

## 在 Pico 上采样

从设置进入 `LENS CALIBRATION`，让完整棋盘格进入预览，按确认保存当前帧。页面显示本次运行已保存的张数。每次拍摄后移动或转动棋盘格，再按确认。应用保存相机帧的完整灰度 PGM，按启动后首次采样顺序存到 `/userdata/calibration-samples/session-N`。在电脑上用 ADB 拉取这次的目录，例如 `adb pull /userdata/calibration-samples/session-1 photos`。

## 标定与预览

安装 Python 3.9 或更新版本以及 OpenCV。在项目根目录运行以下命令，把示例路径换成你的照片目录和文件名。

```sh
python -m pip install opencv-python-headless
python tools/camera_calibration/calibrate.py calibrate photos -o camera.json \
  --device-output camera-calibration.conf
python tools/camera_calibration/calibrate.py preview photos/view01.jpg --calibration camera.json --zoom 1.5 -o preview.png
```

默认按 20 mm 单格边长求解。若纸上的尺寸均匀但实际边长不同，可用 `--square-mm` 输入实测值。若横纵格宽不同，按原尺寸重新打印。

`camera.json` 包含相机内参矩阵、`k1`、`k2`、`p1`、`p2`、`k3` 畸变系数、整体和逐张重投影 RMS（像素）。预览要求原图与标定分辨率一致。`--zoom 1` 保留输出视野，`--zoom 1.5` 取中心约 2/3 的宽高并放回原尺寸。省略 `--calibration` 可单独预览放大效果。建议用一张未参与标定、有明显直线的照片检查去畸变效果。

把 `camera-calibration.conf` 上传后重启程序即可让主界面的轨迹投影使用该镜头参数：

```sh
adb shell mkdir -p /userdata/autoaimer
adb push camera-calibration.conf /userdata/autoaimer/camera-calibration.conf
adb shell /etc/init.d/S98autoaimer restart
```
