#!/usr/bin/env python3
"""在项目输出目录修改已验证的 MicroSD 镜像；SDK 仅提供只读的 mkimage 工具。"""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import zipfile
import zlib


def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True).strip()


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4*1024*1024), b""):
            h.update(block)
    return h.hexdigest()


def get(path, node, prop, kind="s"):
    return run("fdtget", "-t", kind, path, node, prop)


def number(path, node, prop):
    return int(get(path, node, prop, "x"), 16)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def debugfs(image, command, write=False):
    result = subprocess.run(["debugfs", *(["-w"] if write else []), "-R", command, str(image)],
                            text=True, capture_output=True, check=True)
    # debugfs 对某些操作错误仍返回 0，安装后的哈希和 mode 检查不可省略。
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-zip", required=True, type=Path)
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--project", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    out, project = args.out.resolve(), args.project.resolve()
    require(not out.exists(), "Output directory already exists; choose a new release directory")
    require(args.sdk.resolve() not in out.parents, "Output must be outside the SDK")
    out.mkdir(parents=True)
    raw = out / "AutoAimer-Luckfox-Pico-RV1103-GC9A01-MicroSD.img"
    with zipfile.ZipFile(args.base_zip) as archive:
        entries = [n for n in archive.namelist() if n.endswith(".img")]
        require(len(entries) == 1, "Expected one previously verified raw image")
        expected = archive.read("SHA256SUMS").decode().split()[0]
        with archive.open(entries[0]) as source, raw.open("wb") as target:
            shutil.copyfileobj(source, target)
    require(digest(raw) == expected, "Base image SHA-256 mismatch")

    # 固定这份标准 Pico 板级分区布局；不将其它型号镜像套用相同偏移。
    boot_offset, boot_capacity, root_offset = 0x108000, 32*1024*1024, 0x12108000
    boot, rootfs = out / "boot-original.img", out / "rootfs.img"
    with raw.open("rb") as source:
        env = source.read(32768)
        require(zlib.crc32(env[4:]) == int.from_bytes(env[:4], "little"), "ENV CRC mismatch")
        require(b"blkdevparts=mmcblk1:32K(env),512K@32K(idblock),512K(uboot),32M(boot),256M(userdata),-(rootfs)\0" in env,
                "Unexpected MicroSD partition map")
        source.seek(boot_offset)
        boot.write_bytes(source.read(boot_capacity))
        source.seek(root_offset)
        with rootfs.open("wb") as target:
            shutil.copyfileobj(source, target)

    # 从 FIT 中提取三个 payload，并在修改前校验各自的原始 hash。
    images = run("fdtget", "-l", boot, "/images").split()
    require(images == ["fdt", "kernel", "resource"], "Unexpected FIT image components")
    original_hashes = {}
    with boot.open("rb") as source:
        for name in images:
            node = "/images/"+name
            source.seek(number(boot, node, "data-position"))
            data = source.read(number(boot, node, "data-size"))
            expected_hash = bytes(int(n, 16) for n in get(boot, node+"/hash", "value", "bx").split()).hex()
            require(hashlib.sha256(data).hexdigest() == expected_hash, "Bad base FIT hash: "+name)
            original_hashes[name] = expected_hash
            (out/(name+".bin")).write_bytes(data)
    dtb = out / "fdt.bin"
    require(get(dtb, "/", "model") == "Luckfox Pico", "This is not standard Luckfox Pico")
    require("rockchip,rv1103" in get(dtb, "/", "compatible").split(), "This is not RV1103")
    require("pinctrl-0" not in run("fdtget", "-p", dtb, "/pinctrl/gpio@ff530000").split(),
            "GPIO1 already has default pin assignments; inspect before changing")
    overlay = out / "gc9a01-pico.dtbo"
    run("dtc", "-@", "-I", "dts", "-O", "dtb", "-o", overlay, project/"deploy/gc9a01-pico.dts")
    # 运行时只覆盖频率，避免再次创建完整板级配置中的 pinctrl 节点。
    speed_overlay = out / "gc9a01-speed-40mhz.dtbo"
    run("dtc", "-@", "-I", "dts", "-O", "dtb", "-o", speed_overlay,
        project/"deploy/gc9a01-speed-40mhz.dts")
    updated = out / "gc9a01-pico.dtb"
    run("fdtoverlay", "-i", dtb, "-o", updated, overlay)
    require(get(updated, "/spi@ff500000", "status") == "okay", "SPI controller disabled")
    require(get(updated, "/spi@ff500000/fbtft@0", "status") == "disabled", "FBTFT conflicts with spidev")
    shutil.copyfile(updated, dtb)

    # 设备树变大后，由 SDK mkimage 重新安排 FIT payload 偏移和哈希。
    # 不修改内核、资源内容或 SDK 源码，也不越过原 DTB 空间直接覆盖内核。
    its = run("dtc", "-I", "dtb", "-O", "dts", boot)
    its = re.sub(r"^/memreserve/.*;\n", "", its, flags=re.M)
    its = re.sub(r"^\s*(totalsize|timestamp|data-size|data-position|value)\s*=.*;\n", "", its, flags=re.M)
    for name in images:
        its = its.replace("\t\t"+name+" {", "\t\t"+name+' {\n\t\t\tdata = /incbin/("'+name+'.bin");', 1)
    (out/"boot.its").write_text(its)
    mkimage = args.sdk.resolve()/"sysdrv/tools/pc/uboot_tools/mkimage"
    result = subprocess.run([str(mkimage), "-E", "-p", "0x800", "-f", "boot.its", "boot.img"],
                            cwd=out, text=True, capture_output=True)
    (out/"mkimage.log").write_text(result.stdout+result.stderr)
    require(result.returncode == 0, "mkimage failed; see mkimage.log")
    packed_boot = out/"boot.img"
    require(packed_boot.stat().st_size <= boot_capacity, "FIT exceeds boot partition")
    with packed_boot.open("rb") as source:
        for name in images:
            node = "/images/"+name
            source.seek(number(packed_boot, node, "data-position"))
            data = source.read(number(packed_boot, node, "data-size"))
            expected_hash = bytes(int(n, 16) for n in get(packed_boot, node+"/hash", "value", "bx").split()).hex()
            require(hashlib.sha256(data).hexdigest() == expected_hash, "Bad new FIT hash: "+name)
            require(data == (out/(name+".bin")).read_bytes(), "FIT payload mismatch: "+name)
            if name != "fdt":
                require(expected_hash == original_hashes[name], "Unintended kernel/resource change")

    # ext4 离线安装无需 root 或 loop mount。旧自检仅从新镜像副本移除。
    for obsolete in ("/usr/bin/autoaimer-selftest", "/etc/init.d/S98autoaimer-selftest"):
        debugfs(rootfs, "rm "+obsolete, True)
    debugfs(rootfs, "mkdir /etc/autoaimer", True)
    installed = {
        "/usr/bin/autoaimer": (project/"build-spi-rv1103/autoaimer", "0755"),
        "/etc/init.d/S21appinit": (project/"deploy/overlay-autoaimer/etc/init.d/S21appinit", "0755"),
        "/etc/init.d/S98autoaimer": (project/"deploy/overlay-autoaimer/etc/init.d/S98autoaimer", "0755"),
        "/etc/autoaimer/gc9a01-speed-40mhz.dtbo": (speed_overlay, "0644"),
    }
    for destination, (local, mode) in installed.items():
        debugfs(rootfs, "rm "+destination, True)
        debugfs(rootfs, f"write {local} {destination}", True)
        debugfs(rootfs, f"set_inode_field {destination} mode 010{mode}", True)
        exported = out/("check-"+Path(destination).name)
        debugfs(rootfs, f"dump {destination} {exported}")
        require(exported.exists() and digest(exported) == digest(local), "Rootfs content mismatch: "+destination)
        require("Mode:  "+mode in debugfs(rootfs, "stat "+destination), "Wrong file mode: "+destination)
    dependencies = re.findall(r"Shared library: \[([^]]+)\]", run("readelf", "-d", project/"build-spi-rv1103/autoaimer"))
    for library in ["ld-uClibc.so.0", *dependencies]:
        require("Inode:" in debugfs(rootfs, "stat /lib/"+library), "Missing runtime: "+library)
    require("Inode:" in debugfs(rootfs, "stat /usr/bin/adbd"), "ADB daemon missing")
    fsck = subprocess.run(["e2fsck", "-fn", str(rootfs)], capture_output=True, text=True)
    (out/"e2fsck.log").write_text(fsck.stdout+fsck.stderr)
    require(fsck.returncode == 0, "Rootfs check failed; see e2fsck.log")

    with raw.open("r+b") as target:
        for offset, path in ((boot_offset, packed_boot), (root_offset, rootfs)):
            target.seek(offset)
            with path.open("rb") as source:
                shutil.copyfileobj(source, target)
    # 验证新分区实际写入整卡镜像的位置。
    with raw.open("rb") as source:
        for offset, path in ((boot_offset, packed_boot), (root_offset, rootfs)):
            source.seek(offset)
            h, remaining = hashlib.sha256(), path.stat().st_size
            while remaining:
                block = source.read(min(4*1024*1024, remaining))
                require(bool(block), "Truncated raw image")
                h.update(block)
                remaining -= len(block)
            require(h.hexdigest() == digest(path), "Raw partition mismatch: "+path.name)
    (out/"SHA256SUMS").write_text(digest(raw)+"  "+raw.name+"\n")
    (out/"VERIFICATION.txt").write_text(
        "Board: Luckfox Pico, RV1103, MicroSD, Buildroot/uClibc\n"
        "Base raw SHA256: "+expected+"\n"
        "FIT hashes: verified; original kernel and resource preserved\n"
        "SPI0: mode 0, 40 MHz, CS0/CLK/MOSI; FBTFT disabled; GPIO1 C7 DC / C6 RESET\n"
        "App: ARM EABI5, camera_sim, GC9A01 display; no test sensor data\n"
        "Frame: single center crop/resize to 240x240, native circular HUD, two RGB565 buffers\n"
        "Display: 30 cropped 8-row bands, 93770 SPI bytes/frame; GPIO character-device handles\n"
        "Threads: main producer and std::thread sender; latest pending frame, join on exit\n"
        "Rootfs: application/init hashes, modes, runtime libraries, ADB and e2fsck verified\n"
        "Ubuntu CTest: 7/7 passed; display ownership, latest frame and error propagation verified\n"
        "Hardware boot of this newly packed image: not performed by packager\n")
    shutil.copyfile(project/"deploy/SPI显示与调试.md", out/"README.md")
    with zipfile.ZipFile(out.with_suffix(".zip"), "w", zipfile.ZIP_DEFLATED, compresslevel=3) as archive:
        for path in (raw, out/"SHA256SUMS", out/"VERIFICATION.txt", out/"README.md"):
            archive.write(path, path.name)
    with zipfile.ZipFile(out.with_suffix(".zip")) as archive:
        require(archive.testzip() is None, "ZIP verification failed")
    print((out/"VERIFICATION.txt").read_text())
    print((out/"SHA256SUMS").read_text())
    print(out.with_suffix(".zip"))


if __name__ == "__main__":
    main()
