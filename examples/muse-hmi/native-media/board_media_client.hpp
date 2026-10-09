// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "vendor/json.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>
namespace muse_board {
inline nlohmann::json call(const std::string& action,const nlohmann::json& params){
 int fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(8765);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(fd<0||connect(fd,(sockaddr*)&a,sizeof(a))){if(fd>=0)close(fd);throw std::runtime_error("board API unavailable");}timeval timeout{3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));std::string body=nlohmann::json({{"action",action},{"params",params}}).dump(),request="POST /media HTTP/1.1\r\nConnection: close\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;size_t at=0;while(at<request.size()){auto n=write(fd,request.data()+at,request.size()-at);if(n<=0){close(fd);throw std::runtime_error("board API write failed");}at+=n;}std::string response;char buffer[4096];ssize_t n;while((n=read(fd,buffer,sizeof(buffer)))>0){response.append(buffer,n);if(response.size()>1048576)break;}close(fd);auto header=response.find("\r\n\r\n");if(header==std::string::npos)throw std::runtime_error("board API response missing");return nlohmann::json::parse(response.substr(header+4));
}
}
