// SPDX-License-Identifier: Apache-2.0
// Measure the board API and the first fully rendered frame, without SSH latency.
#include "vendor/json.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <iostream>
using J=nlohmann::json;
static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static J api(const std::string& action,const J& params=J::object()){
 int fd=socket(AF_INET,SOCK_STREAM,0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(8765);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(connect(fd,(sockaddr*)&a,sizeof(a)))throw std::runtime_error("board API unavailable");timeval timeout{15,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));std::string body=J({{"action",action},{"params",params}}).dump(),request="POST /media HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;size_t at=0;while(at<request.size()){auto n=write(fd,request.data()+at,request.size()-at);if(n<=0)throw std::runtime_error("write failed");at+=n;}std::string response;char buffer[4096];ssize_t n;while((n=read(fd,buffer,sizeof(buffer)))>0)response.append(buffer,n);close(fd);return J::parse(response.substr(response.find("\r\n\r\n")+4));
}
int main(){try{auto original=api("media.status")["payload"].value("media_kind",std::string("text"));if(original=="youtube"||original=="animation")original="video";if(original=="none")original="text";for(int round=0;round<2;round++)for(auto kind:{"text","image","pdf"}){double started=now();auto accepted=api("media.switch",{{"kind",kind}});double accepted_at=now();J result={{"kind",kind},{"round",round},{"accept_ms",int((accepted_at-started)*1000)}};if(!accepted.value("ok",false)){result["error"]=accepted;std::cout<<result<<std::endl;continue;}while(now()-started<15){auto status=api("media.status")["payload"];if(status["media_kind"]!=kind){result["interrupted_by"]=status["media_kind"];break;}if(status["state"]=="error"){result["error"]=status["error"];break;}if(status["frames"].get<int>()>0&&(status["state"]=="ready"||status["state"]=="playing")){result["first_frame_ms"]=int((now()-started)*1000);break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}std::cout<<result<<std::endl;}api("media.switch",{{"kind",original}});return 0;}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
