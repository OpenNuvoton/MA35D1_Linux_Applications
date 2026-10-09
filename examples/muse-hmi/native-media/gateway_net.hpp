// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <openssl/ssl.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <array>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline std::string b64(const unsigned char* p,size_t n){std::string out(4*((n+2)/3),'\0');EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]),p,n);return out;}
inline std::string urlquote(const std::string& s){std::string out;const char* hex="0123456789ABCDEF";for(unsigned char c:s){if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=c;else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
struct TLS {
 int fd=-1;SSL_CTX* ctx=nullptr;SSL* ssl=nullptr;
 TLS(const std::string& host,const std::string& ca,int port=443){
  try{ctx=SSL_CTX_new(TLS_client_method());require(ctx,"TLS allocation failed");SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION);SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,nullptr);require(SSL_CTX_load_verify_locations(ctx,ca.c_str(),nullptr)==1,"CA certificate unavailable");
  addrinfo hints{},*all=nullptr;hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;require(getaddrinfo(host.c_str(),std::to_string(port).c_str(),&hints,&all)==0,"DNS lookup failed");
  for(auto a=all;a;a=a->ai_next){fd=socket(a->ai_family,SOCK_STREAM|SOCK_CLOEXEC,0);if(fd<0)continue;timeval timeout{15,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));if(connect(fd,a->ai_addr,a->ai_addrlen)==0)break;close(fd);fd=-1;}freeaddrinfo(all);require(fd>=0,"connection failed");
  ssl=SSL_new(ctx);require(ssl,"TLS allocation failed");require(SSL_set_tlsext_host_name(ssl,host.c_str())==1&&SSL_set1_host(ssl,host.c_str())==1,"TLS hostname setup failed");SSL_set_fd(ssl,fd);require(SSL_connect(ssl)==1&&SSL_get_verify_result(ssl)==X509_V_OK,"TLS verification failed");
  }catch(...){cleanup();throw;}
 }
 void cleanup(){if(ssl)SSL_free(ssl);if(ctx)SSL_CTX_free(ctx);if(fd>=0)close(fd);ssl=nullptr;ctx=nullptr;fd=-1;}
 ~TLS(){cleanup();}TLS(const TLS&)=delete;
 void write(const void* raw,size_t len){auto p=static_cast<const unsigned char*>(raw);while(len){int n=SSL_write(ssl,p,std::min(len,size_t(16384)));require(n>0,"TLS send failed");p+=n;len-=n;}}
 void write(const std::string& s){write(s.data(),s.size());}
 void read(void* raw,size_t len){auto p=static_cast<unsigned char*>(raw);while(len){int n=SSL_read(ssl,p,std::min(len,size_t(16384)));require(n>0,"TLS receive failed");p+=n;len-=n;}}
 std::string line(){std::string s;char c;while(s.size()<16384){read(&c,1);s+=c;if(c=='\n')return s;}throw std::runtime_error("HTTP line too large");}
 bool ready(int ms){if(SSL_pending(ssl))return true;pollfd p{fd,POLLIN,0};return poll(&p,1,ms)>0;}
};
struct HTTPReply {int status;std::string body;};
inline HTTPReply https(const std::string& host,const std::string& path,const std::string& token,const std::string& ca,const std::string& body="",const std::string& method="GET"){
 require(host.find_first_of("\r\n/")==std::string::npos&&path.find_first_of("\r\n")==std::string::npos&&token.find_first_of("\r\n")==std::string::npos,"invalid HTTP request");
 TLS tls(host,ca);tls.write(method+" "+path+" HTTP/1.1\r\nHost: "+host+"\r\nAuthorization: Bearer "+token+"\r\nX-API-Version: 1.0.0\r\nUser-Agent: musegadget/0.1.0 (MA35 board native)\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
 auto first=tls.line();int status=atoi(first.c_str()+first.find(' ')+1);size_t length=0;bool known=false,chunked=false;
 for(;;){auto line=tls.line();if(line=="\r\n")break;std::transform(line.begin(),line.end(),line.begin(),::tolower);if(line.rfind("content-length:",0)==0){length=std::stoul(line.substr(15));known=true;}if(line.rfind("transfer-encoding:",0)==0&&line.find("chunked")!=std::string::npos)chunked=true;}
 std::string out;constexpr size_t limit=1024*1024;
 if(chunked){for(;;){size_t n=std::stoul(tls.line(),nullptr,16);if(!n)break;require(n<=limit-out.size(),"HTTP response too large");size_t at=out.size();out.resize(at+n);tls.read(&out[at],n);require(tls.line()=="\r\n","invalid chunk framing");}}
 else if(known){require(length<=limit,"HTTP response too large");out.resize(length);if(length)tls.read(&out[0],length);}
 else{char buf[4096];int n;while((n=SSL_read(tls.ssl,buf,sizeof(buf)))>0){require(out.size()+n<=limit,"HTTP response too large");out.append(buf,n);}}
 return {status,out};
}
struct WebSocket {
 TLS tls;
 WebSocket(const std::string& host,const std::string& vm,const std::string& token,const std::string& ca):tls(host,ca){
  require(host.find_first_of("\r\n/")==std::string::npos&&token.find_first_of("\r\n")==std::string::npos,"invalid WebSocket request");
  unsigned char random[16];require(RAND_bytes(random,16)==1,"random unavailable");auto key=b64(random,16);
  tls.write("GET /v1/noise?vm_id="+urlquote(vm)+" HTTP/1.1\r\nHost: "+host+"\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: "+key+"\r\nAuthorization: Bearer "+token+"\r\nUser-Agent: musegadget/0.1.0 (MA35 board native)\r\n\r\n");
  auto status=tls.line();require(status.find(" 101 ")!=std::string::npos,"WebSocket upgrade rejected");std::string accept;
  for(;;){auto line=tls.line();if(line=="\r\n")break;auto colon=line.find(':');auto field=line.substr(0,colon);std::transform(field.begin(),field.end(),field.begin(),::tolower);if(field=="sec-websocket-accept"){accept=line.substr(colon+1);accept.erase(0,accept.find_first_not_of(" \t"));accept.erase(accept.find_last_not_of("\r\n \t")+1);}}
  auto challenge=key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";unsigned char digest[20];SHA1(reinterpret_cast<const unsigned char*>(challenge.data()),challenge.size(),digest);require(accept==b64(digest,20),"invalid WebSocket accept");
 }
 void send(const unsigned char* p,size_t n,int opcode=2){
  require(n<=1024*1024,"WebSocket send too large");std::vector<unsigned char> out;out.push_back(0x80|opcode);
  if(n<126)out.push_back(0x80|n);else if(n<=65535){out.push_back(0xfe);out.push_back(n>>8);out.push_back(n);}else{out.push_back(0xff);for(int i=7;i>=0;i--)out.push_back(uint64_t(n)>>(i*8));}
  unsigned char mask[4];require(RAND_bytes(mask,4)==1,"random unavailable");out.insert(out.end(),mask,mask+4);for(size_t i=0;i<n;i++)out.push_back(p[i]^mask[i%4]);tls.write(out.data(),out.size());
 }
 std::vector<unsigned char> receive(){
  std::vector<unsigned char> message;bool started=false;
  for(;;){unsigned char h[2];tls.read(h,2);int op=h[0]&15;bool final=h[0]&128;require(!(h[0]&112)&&!(h[1]&128),"invalid WebSocket frame");uint64_t n=h[1]&127;
   if(n==126){unsigned char len[2];tls.read(len,2);n=(len[0]<<8)|len[1];}else if(n==127){unsigned char len[8];tls.read(len,8);n=0;for(auto c:len)n=(n<<8)|c;}
   require(n<=1024*1024-message.size(),"WebSocket frame too large");if(op>=8)require(final&&n<=125,"invalid WebSocket control frame");std::vector<unsigned char> frame(n);if(n)tls.read(frame.data(),n);
   if(op==8)throw std::runtime_error("WebSocket closed");if(op==9){send(frame.data(),n,10);if(!started)return {};continue;}if(op==10){if(!started)return {};continue;}
   require((!started&&op==2)||(started&&op==0),"expected binary WebSocket message");started=true;message.insert(message.end(),frame.begin(),frame.end());if(final)return message;
  }
 }
};
