#!/usr/bin/env python3
"""Write a verified raw image to the explicitly selected removable USB SD card."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import time


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(stream, count):
    h = hashlib.sha256()
    while count:
        block = stream.read(min(count, 4*1024*1024))
        require(bool(block), "Unexpected end of image/device")
        h.update(block)
        count -= len(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--device", required=True, type=Path)
    parser.add_argument("--expected-bytes", required=True, type=int)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    require(str(args.device).startswith("/dev/disk/by-id/usb-"), "Use the inspected USB by-id path")
    device = args.device.resolve(strict=True)
    require(stat.S_ISBLK(device.stat().st_mode), "Target is not a block device")
    info = json.loads(subprocess.check_output([
        "lsblk", "-b", "-J", "-o", "PATH,SIZE,TYPE,TRAN,RM,MOUNTPOINTS", str(device)], text=True))["blockdevices"][0]
    require(info["type"] == "disk" and info["tran"] == "usb" and info["rm"], "Target must be a removable USB disk")
    require(int(info["size"]) == args.expected_bytes, "Target capacity has changed")
    def unmounted(node):
        return not any(node.get("mountpoints") or []) and all(unmounted(c) for c in node.get("children", []))
    require(unmounted(info), "Target has a mounted filesystem; unmount it before writing")
    size = args.image.stat().st_size
    require(size < args.expected_bytes, "Image exceeds target capacity")
    expected, name = (args.image.parent/"SHA256SUMS").read_text().split()
    require(name == args.image.name, "Checksum file does not describe this image")
    with args.image.open("rb") as source:
        require(sha256(source, size) == expected, "Source image SHA-256 mismatch")
    preview = {"device_by_id": str(args.device), "resolved_device": str(device),
               "device_bytes": args.expected_bytes, "image": str(args.image),
               "image_bytes": size, "sha256": expected}
    print(json.dumps(preview), flush=True)
    if not args.apply:
        return
    # O_EXCL ensures a mounted/in-use block device cannot be opened for this write.
    fd = os.open(device, os.O_RDWR | os.O_EXCL)
    started = time.time()
    try:
        actual = struct.unpack("Q", fcntl.ioctl(fd, 0x80081272, bytes(8)))[0]  # BLKGETSIZE64
        require(actual == args.expected_bytes, "Device size changed before opening")
        with os.fdopen(fd, "r+b", buffering=0, closefd=False) as target, args.image.open("rb") as source:
            for block in iter(lambda: source.read(4*1024*1024), b""):
                require(target.write(block) == len(block), "Short device write; not retrying")
            os.fsync(fd)
            # Flush and invalidate block cache before the verification read.
            fcntl.ioctl(fd, 0x1261)  # BLKFLSBUF
            target.seek(0)
            actual_hash = sha256(target, size)
        require(actual_hash == expected, "SD readback SHA-256 mismatch")
        report = {**preview, "readback_sha256": actual_hash, "verified": True,
                  "seconds": round(time.time()-started, 2)}
        args.report.write_text(json.dumps(report, indent=2)+"\n")
        print(json.dumps(report), flush=True)
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
