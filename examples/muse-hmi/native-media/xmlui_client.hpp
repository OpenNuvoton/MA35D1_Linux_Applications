// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "vendor/json.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
inline nlohmann::json xmlui_call(const std::string& action,const nlohmann::json& params){
 int fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);if(fd<0)throw std::runtime_error("XML UI socket unavailable");timeval t{12,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof(t));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof(t));sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(9877);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(connect(fd,(sockaddr*)&addr,sizeof(addr))){close(fd);throw std::runtime_error("Smart home XML UI is not running");}auto q=nlohmann::json({{"action",action},{"params",params}}).dump()+"\n";size_t at=0;while(at<q.size()){auto n=write(fd,q.data()+at,q.size()-at);if(n<=0){close(fd);throw std::runtime_error("XML UI send failed");}at+=n;}std::string out;char b[8192];ssize_t n;while((n=read(fd,b,sizeof(b)))>0){out.append(b,n);if(out.size()>1048576){close(fd);throw std::runtime_error("XML UI reply too large");}if(out.find('\n')!=std::string::npos)break;}close(fd);return nlohmann::json::parse(out);
}
