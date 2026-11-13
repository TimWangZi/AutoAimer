#!/bin/sh
# Load the installed Luckfox camera/media drivers without starting rkipc.
set -e
cd /oem/usr/ko

load_module() {
    name=$1
    shift
    kernel_name=$(printf '%s' "$name" | tr '-' '_')
    if [ ! -d "/sys/module/$kernel_name" ]; then
        insmod "./$name.ko" "$@"
    fi
}

load_module rk_dvbm
load_module sc3336
if [ ! -L /sys/bus/i2c/drivers/sc3336/4-0030 ]; then
    echo 'SC3336 did not bind on I2C bus 4 at 0x30' >&2
    exit 1
fi
load_module video_rkcif
load_module video_rkisp
load_module phy-rockchip-csi2-dphy-hw
load_module phy-rockchip-csi2-dphy
# 清理未安装传感器的媒体路由。后续启动保留已建立的路由。
if [ ! -d /sys/module/rockit ]; then
    echo 1 > /sys/module/video_rkcif/parameters/clr_unready_dev
    echo 1 > /sys/module/video_rkisp/parameters/clr_unready_dev
fi
load_module rga3
load_module mpp_vcodec
load_module rockit mcu_fw_path=./hpmcu_wrap.bin mcu_fw_addr=0xff6fe000 isp_max_h=1296
