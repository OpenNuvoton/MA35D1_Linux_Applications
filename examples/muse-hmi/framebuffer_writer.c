// SPDX-License-Identifier: Apache-2.0
#define _XOPEN_SOURCE 700
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define STRIDE 4096
static int read_all(void *buf, size_t size) {
    unsigned char *p=buf;
    while(size) { ssize_t n=read(0,p,size); if(n<0 && errno==EINTR)continue; if(n<=0)return -1; p+=n;size-=n; }
    return 0;
}
int main(int argc,char **argv) {
    int fd=open(argc>1?argv[1]:"/dev/fb0",O_RDWR);
    if(fd<0){perror("framebuffer open");return 1;}
    uint32_t count,rect[4];unsigned char row[STRIDE];
    while(!read_all(&count,4)) {
        int native_active=access("/tmp/muse-native-media.active",F_OK)==0;
        if(count>512)return 2;
        for(uint32_t i=0;i<count;i++) {
            if(read_all(rect,16))return 3;
            uint32_t x=rect[0],y=rect[1],w=rect[2]&0x7fffffff,h=rect[3];
            if(x>=1024||y>=600||!w||!h||w>1024-x||h>600-y)return 4;
            if(rect[2]&0x80000000) {
                int32_t shift;if(read_all(&shift,4))return 8;
                if(!shift || shift>=(int32_t)h || shift<=-(int32_t)h)return 9;
                if(native_active)continue;
                int first=shift>0?(int)h-1:0,last=shift>0?shift-1:(int)h+shift;
                int step=shift>0?-1:1;
                for(int j=first;j!=last;j+=step) {
                    off_t src=(off_t)(y+j-shift)*STRIDE+x*4;
                    off_t dst=(off_t)(y+j)*STRIDE+x*4;
                    if(pread(fd,row,w*4,src)!=(ssize_t)(w*4))return 10;
                    if(pwrite(fd,row,w*4,dst)!=(ssize_t)(w*4))return 11;
                }
                continue;
            }
            for(uint32_t j=0;j<h;j++) {
                size_t size=w*4;if(read_all(row,size))return 5;
                if(native_active)continue;
                off_t offset=(off_t)(y+j)*STRIDE+x*4;size_t done=0;
                while(done<size) {
                    ssize_t n=pwrite(fd,row+done,size-done,offset+done);
                    if(n<0 && errno==EINTR)continue;
                    if(n<=0){perror("framebuffer write");return 6;}done+=n;
                }
            }
        }
        if(write(1,native_active?"N":"K",1)!=1)return 7;
    }
    close(fd);return 0;
}
