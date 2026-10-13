#!/bin/sh
# Never formats NAND. Loads the bounded partition helper and mounts existing UBI.
set -eu
base=/mnt/sd/muse/nand-tools
if [ ! -f /sys/class/mtd/mtd5/name ]; then
 /sbin/insmod "$base/ma35_nand_data.ko"
fi
[ "$(cat /sys/class/mtd/mtd5/name)" = nand-data ]
[ "$(cat /sys/class/mtd/mtd5/size)" = 939524096 ]
[ "$(cat /sys/class/mtd/mtd5/offset)" = 134217728 ]
if [ ! -d /sys/class/ubi/ubi1 ]; then
 "$base/ubiattach" /dev/ubi_ctrl -m 5 -d 1
fi
[ "$(cat /sys/class/ubi/ubi1/mtd_num)" = 5 ]
[ "$(cat /sys/class/ubi/ubi1_0/name)" = appdata ]
mkdir -p /mnt/nand
if ! grep -qs ' /mnt/nand ' /proc/mounts; then
 mount -t ubifs -o noatime,nosuid,nodev ubi1:appdata /mnt/nand
fi
