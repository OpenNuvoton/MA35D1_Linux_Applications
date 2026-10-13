// SPDX-License-Identifier: GPL-2.0
/* Expose the unused tail as a separate MTD partition; never erase in this module. */
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/sizes.h>

static struct mtd_info *root_mtd;
static int added_index = -1;
static int __init nand_data_init(void)
{
 struct mtd_info *master, *child, *data;
 const u64 start = 128ULL * SZ_1M, length = 896ULL * SZ_1M;
 int ret;
 root_mtd = get_mtd_device_nm("nand-rootfs");
 if (IS_ERR(root_mtd)) return PTR_ERR(root_mtd);
 master = mtd_get_master(root_mtd);
 if (master->size != 1024ULL * SZ_1M || master->type != MTD_NANDFLASH ||
     master->erasesize != SZ_128K || mtd_get_master_ofs(root_mtd,0) != 28ULL * SZ_1M ||
     root_mtd->size != 100ULL * SZ_1M) { ret = -EINVAL; goto release; }
 /* The existing fixed partitions must all finish below the new region. */
 list_for_each_entry(child, &master->partitions, part.node) {
  if (mtd_get_master_ofs(child,0) + child->part.size > start) {
   ret = -EEXIST; goto release;
  }
 }
 ret = mtd_add_partition(master,"nand-data",start,length);
 if (ret) goto release;
 data = get_mtd_device_nm("nand-data");
 if (IS_ERR(data)) { ret = PTR_ERR(data); goto release; }
 added_index = data->index;
 put_mtd_device(data);
 pr_info("ma35_nand_data: exposed 896 MiB at offset 128 MiB as mtd%d; no flash erased\n",added_index);
 return 0;
release:
 put_mtd_device(root_mtd); root_mtd = NULL;
 return ret;
}
static void __exit nand_data_exit(void)
{
 if (root_mtd) {
  if (added_index >= 0) {
   int ret = mtd_del_partition(mtd_get_master(root_mtd),added_index);
   if (ret) pr_warn("ma35_nand_data: data partition remains in use (%d)\n",ret);
  }
  put_mtd_device(root_mtd);
 }
}
module_init(nand_data_init);
module_exit(nand_data_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Expose the unused MA35H0 W29N08GV NAND tail as data storage");
