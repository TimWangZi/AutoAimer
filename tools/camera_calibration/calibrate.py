#!/usr/bin/env python3
"""从棋盘格照片标定普通相机，并导出照片的去畸变和放大预览。"""

import argparse
import json
import math
from pathlib import Path
import sys

import cv2
import numpy as np


INNER_CORNERS = (9, 6)  # 列数、行数，与 A4 打印件对应。
IMAGE_EXTENSIONS = {'.bmp', '.jpeg', '.jpg', '.pgm', '.png', '.ppm', '.tif', '.tiff'}
MIN_VIEWS = 10


def read_image(path, mode):
    # imdecode 保留 Windows 下中文路径的支持。忽略 EXIF 旋转以保留像素坐标。
    data = np.fromfile(path, dtype=np.uint8)
    image = cv2.imdecode(data, mode | cv2.IMREAD_IGNORE_ORIENTATION) if data.size else None
    if image is None:
        raise ValueError(f'无法读取图片 {path}')
    return image


def calibrate(args):
    square_mm = args.square_mm
    if not math.isfinite(square_mm) or square_mm <= 0:
        raise ValueError('棋盘格边长必须为正数，单位为 mm')

    columns, rows = INNER_CORNERS
    board = np.zeros((columns * rows, 3), dtype=np.float32)
    board[:, :2] = np.mgrid[:columns, :rows].T.reshape(-1, 2) * square_mm

    views = []
    names = []
    image_size = None
    for path in sorted(args.directory.iterdir()):
        if not path.is_file() or path.suffix.lower() not in IMAGE_EXTENSIONS:
            continue
        gray = read_image(path, cv2.IMREAD_GRAYSCALE)
        size = (gray.shape[1], gray.shape[0])
        if image_size is not None and size != image_size:
            raise ValueError(f'{path.name} 的分辨率不同，请使用同一分辨率的原始相机照片')
        image_size = size
        found, corners = cv2.findChessboardCornersSB(gray, INNER_CORNERS,
                                                    cv2.CALIB_CB_NORMALIZE_IMAGE)
        if found:
            views.append(corners)
            names.append(path.name)
        else:
            print(f'未找到完整棋盘格，跳过 {path.name}', file=sys.stderr)

    if len(views) < MIN_VIEWS:
        raise ValueError(f'只有 {len(views)} 张照片检测到棋盘格，至少需要 {MIN_VIEWS} 张')

    # 默认针孔模型拟合 k1、k2、p1、p2、k3，并返回每张照片的重投影误差。
    rms, matrix, distortion, _, _, _, _, errors = cv2.calibrateCameraExtended(
        [board] * len(views), views, image_size, None, None)
    if not (math.isfinite(rms) and np.isfinite(matrix).all() and np.isfinite(distortion).all()
            and np.isfinite(errors).all() and matrix[0, 0] > 0 and matrix[1, 1] > 0):
        raise ValueError('标定没有得到有效结果，请增加不同位置和倾角的棋盘格照片')

    result = {
        'image_size': list(image_size),
        'camera_matrix': matrix.tolist(),
        'distortion_coefficients': distortion.ravel().tolist(),
        'inner_corners': list(INNER_CORNERS),
        'square_size_mm': square_mm,
        'rms_pixels': rms,
        'views': [{'image': name, 'rms_pixels': float(error)}
                  for name, error in zip(names, errors.ravel())],
    }
    args.output.write_text(json.dumps(result, ensure_ascii=False, allow_nan=False, indent=2) + '\n',
                           encoding='utf-8')
    if args.device_output:
        # 板端只需要固定的小型键值文件，避免在 RV1103 上引入 JSON 解析库。
        coefficients = np.pad(distortion.ravel(), (0, max(0, 5 - distortion.size)))
        device = {
            'format': 'autoaimer-camera-v1',
            'image_width': image_size[0],
            'image_height': image_size[1],
            'fx': matrix[0, 0],
            'fy': matrix[1, 1],
            'cx': matrix[0, 2],
            'cy': matrix[1, 2],
            'k1': coefficients[0],
            'k2': coefficients[1],
            'p1': coefficients[2],
            'p2': coefficients[3],
            'k3': coefficients[4],
            'rms_pixels': rms,
        }
        args.device_output.write_text(
            ''.join(f'{key}={value}\n' for key, value in device.items()), encoding='ascii')
        print(f'板端参数已保存至 {args.device_output}')
    print(f'{len(views)} 张照片参与标定，重投影 RMS {rms:.3f} px，结果已保存至 {args.output}')


def preview(args):
    if not math.isfinite(args.zoom) or args.zoom < 1:
        raise ValueError('放大倍数必须大于或等于 1')
    if args.output.suffix.lower() != '.png':
        raise ValueError('预览输出路径必须使用 .png 扩展名')
    if args.image.resolve() == args.output.resolve():
        raise ValueError('请为预览指定不同于原图的输出路径')

    image = read_image(args.image, cv2.IMREAD_COLOR)
    height, width = image.shape[:2]
    if args.calibration:
        calibration = json.loads(args.calibration.read_text(encoding='utf-8'))
        if calibration['image_size'] != [width, height]:
            raise ValueError('照片分辨率必须与标定参数一致')
        matrix = np.asarray(calibration['camera_matrix'], dtype=np.float64)
        distortion = np.asarray(calibration['distortion_coefficients'], dtype=np.float64)
        image = cv2.undistort(image, matrix, distortion)

    if args.zoom > 1:
        # 取中心区域并放回原图尺寸。放大倍数取决于裁剪比例。
        crop_width = max(1, round(width / args.zoom))
        crop_height = max(1, round(height / args.zoom))
        x = (width - crop_width) // 2
        y = (height - crop_height) // 2
        image = cv2.resize(image[y:y + crop_height, x:x + crop_width], (width, height),
                           interpolation=cv2.INTER_LINEAR)

    _, encoded = cv2.imencode('.png', image)
    encoded.tofile(args.output)
    print(f'预览已保存至 {args.output}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(required=True)

    command = commands.add_parser('calibrate', help='从棋盘格照片估计相机内参和畸变')
    command.add_argument('directory', type=Path, help='存放原始照片的目录')
    command.add_argument('-o', '--output', type=Path, required=True, help='保存标定参数的 JSON 路径')
    command.add_argument('--device-output', type=Path,
                         help='同时导出可上传到 AutoAimer 的轻量配置文件')
    command.add_argument('--square-mm', type=float, default=20, help='实测单格边长，默认 20 mm')
    command.set_defaults(run=calibrate)

    command = commands.add_parser('preview', help='导出照片的去畸变或居中放大预览')
    command.add_argument('image', type=Path)
    command.add_argument('-o', '--output', type=Path, required=True, help='输出 PNG 文件')
    command.add_argument('--calibration', type=Path, help='用于去畸变的标定 JSON 文件')
    command.add_argument('--zoom', type=float, default=1, help='居中放大倍率，默认 1')
    command.set_defaults(run=preview)

    args = parser.parse_args()
    try:
        args.run(args)
    except (OSError, ValueError, cv2.error, KeyError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
