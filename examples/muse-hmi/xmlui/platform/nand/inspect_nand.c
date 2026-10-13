// SPDX-License-Identifier: MIT
// Read every good block of the new data partition without writing flash.
#define _FILE_OFFSET_BITS 64
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 int fd=open(argv[1],O_RDONLY);struct mtd_info_user info;
 if(fd<0||ioctl(fd,MEMGETINFO,&info)){perror("MTD info");return 2;}
 if(info.size!=896U*1024*1024||info.erasesize!=128U*1024||info.type!=MTD_NANDFLASH){fprintf(stderr,"unexpected data geometry\n");return 2;}
 unsigned char *buf=malloc(info.erasesize);unsigned bad=0,nonempty=0;uint64_t first=UINT64_MAX;
 for(loff_t off=0;off<info.size;off+=info.erasesize){
  int b=ioctl(fd,MEMGETBADBLOCK,&off);if(b<0){perror("bad block query");return 2;}if(b){bad++;continue;}
  ssize_t n=pread(fd,buf,info.erasesize,off);if(n!=(ssize_t)info.erasesize){perror("NAND read");return 2;}
  int empty=1;for(unsigned i=0;i<info.erasesize;i++)if(buf[i]!=255){empty=0;if(first==UINT64_MAX)first=off+i;break;}
  if(!empty)nonempty++;
  if((off+info.erasesize)%(128U*1024*1024)==0){printf("Scanned %llu MiB\n",(unsigned long long)(off+info.erasesize)/(1024*1024));fflush(stdout);}
 }
 printf("{\"size_mib\":896,\"bad_blocks\":%u,\"nonempty_blocks\":%u,\"all_good_blocks_erased\":%s,\"first_nonempty_offset\":%lld}\n",bad,nonempty,nonempty?"false":"true",first==UINT64_MAX?-1:(long long)first);
 free(buf);close(fd);return nonempty?1:0;
}
