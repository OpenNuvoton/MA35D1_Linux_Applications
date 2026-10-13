# MA35H0 NAND data volume

The board has Winbond W29N08GV SLC NAND, 1024 MiB. Its existing five fixed
partitions cover only the first 128 MiB. `ma35_nand_data.ko` exposes the remaining
896 MiB as `nand-data` through Linux MTD, without rewriting boot flash.
It checks the chip geometry, root partition location and overlap before adding
the partition. It performs no erase operation. The module is built against the
board-specific vendor Linux 5.10.140 configuration.

On 2026-10-12, the read-only scanner checked every good block: four bad blocks,
zero nonempty good blocks. After that check the new partition alone was formatted
with UBI and an `appdata` dynamic volume was created. UBIFS mounts it at
`/mnt/nand`; initial usable space was about 777 MiB. Root remains `ubi0:rootfs`;
the data volume is `ubi1:appdata`. Existing boot metadata backups are under
`artifacts/nand_data_20261012`.

`mount-data.sh` and `nand-data.service` only expose/attach/mount the existing
volume at boot. They never format it. The small module and UBI tools currently
reside on SD under `/mnt/sd/muse/nand-tools`, so the SD mount is a prerequisite.

The shared Qt/LVGL demo, Qt dependencies and XML templates are installed beneath
`/mnt/nand/apps/smart-home-overlay`. Large existing media caches and SD swap
remain on SD. Do not run initial format commands again on this populated volume.
