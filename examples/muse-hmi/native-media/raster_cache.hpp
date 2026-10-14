// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
namespace muse_media {
// Cache decoded pixels on SD; mmap on revisit avoids PNG/PDF decode and heap copies.
struct RasterCache {
 void* mapping=MAP_FAILED;size_t bytes=0;int width=0,height=0;
 ~RasterCache(){clear();}
 void clear(){if(mapping!=MAP_FAILED)munmap(mapping,bytes);mapping=MAP_FAILED;bytes=0;}
 bool open(const std::string& path){clear();if(path.empty())return false;int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC);if(fd<0)return false;struct stat st{};uint32_t header[4]{};bool valid=fstat(fd,&st)==0&&read(fd,header,sizeof(header))==sizeof(header)&&header[0]==0x31424752&&header[1]>0&&header[1]<=4096&&header[2]>0&&header[2]<=4096&&uint64_t(header[1])*header[2]<=4000000&&uint64_t(st.st_size)==16+uint64_t(header[1])*header[2]*3;if(!valid){::close(fd);return false;}bytes=st.st_size;mapping=mmap(nullptr,bytes,PROT_READ,MAP_PRIVATE,fd,0);::close(fd);if(mapping==MAP_FAILED){bytes=0;return false;}width=header[1];height=header[2];return true;}
 const unsigned char* pixels()const{return static_cast<const unsigned char*>(mapping)+16;}
 static void write(const std::string& path,int width,int height,const unsigned char* data){if(path.empty())return;uint32_t header[4]={0x31424752,uint32_t(width),uint32_t(height),0};std::string temp=path+".new-"+std::to_string(getpid());std::ofstream f(temp,std::ios::binary);f.write(reinterpret_cast<const char*>(header),16);f.write(reinterpret_cast<const char*>(data),size_t(width)*height*3);f.close();if(f)rename(temp.c_str(),path.c_str());else unlink(temp.c_str());}
};
}
