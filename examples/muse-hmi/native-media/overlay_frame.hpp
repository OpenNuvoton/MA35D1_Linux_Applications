// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
// The native media process publishes its pixels; only the compositor writes fb0.
inline bool xml_overlay_active(){return access("/tmp/muse-xmlui.active",F_OK)==0;}
inline void publish_overlay_frame(const void* pixels,size_t count,const char* path="/tmp/muse-xmlui-frame"){
 static int fd=-1;static unsigned char* map=nullptr;
 if(!map){fd=open(path,O_RDWR|O_CREAT|O_CLOEXEC,0600);if(fd<0)return;if(ftruncate(fd,16+count)){close(fd);fd=-1;return;}map=(unsigned char*)mmap(nullptr,16+count,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(map==MAP_FAILED){map=nullptr;close(fd);fd=-1;return;}auto h=(uint32_t*)map;if(h[0]!=0x55494652){h[0]=0x55494652;h[1]=0;}else if(h[1]&1)__atomic_add_fetch(h+1,1,__ATOMIC_RELEASE);h[2]=1024;h[3]=600;}
 auto seq=(uint32_t*)map+1;__atomic_add_fetch(seq,1,__ATOMIC_SEQ_CST);memcpy(map+16,pixels,count);__atomic_add_fetch(seq,1,__ATOMIC_RELEASE);
}
