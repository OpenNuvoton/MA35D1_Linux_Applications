// SPDX-License-Identifier: Apache-2.0
// IPv4-only resolver for the ICS board. Avoids loading incompatible dynamic NSS
// modules into static codec binaries. All data is per-call for threaded curl.
#define _POSIX_C_SOURCE 200809L
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/random.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static unsigned u16(const unsigned char*p){return ((unsigned)p[0]<<8)|p[1];}
static int skip(const unsigned char *b,size_t n,size_t *p){for(int i=0;i<128&&*p<n;i++){unsigned c=b[(*p)++];if(!c)return 0;if((c&192)==192){if(*p>=n)return -1;(*p)++;return 0;}if(c>63||*p+c>n)return -1;*p+=c;}return -1;}
static int lookup(const char*name,struct in_addr*ip){
 if(inet_pton(AF_INET,name,ip)==1)return 0;
 if(!strcmp(name,"localhost")){ip->s_addr=htonl(INADDR_LOOPBACK);return 0;}
 unsigned char q[512]={0},answer[4096];uint16_t id=0;if(getrandom(&id,2,0)!=2)id=(uint16_t)getpid();q[0]=id>>8;q[1]=id;q[2]=1;q[5]=1;size_t p=12;const char*start=name;
 while(*start){const char*dot=strchr(start,'.');size_t size=dot?(size_t)(dot-start):strlen(start);if(!size||size>63||p+size+6>sizeof(q))return EAI_NONAME;q[p++]=(unsigned char)size;memcpy(q+p,start,size);p+=size;if(!dot)break;start=dot+1;}
 q[p++]=0;q[p++]=0;q[p++]=1;q[p++]=0;q[p++]=1;
 char server[64]="192.168.137.1";FILE*f=fopen("/etc/resolv.conf","r");if(f){char line[256],candidate[64];while(fgets(line,sizeof(line),f)){if(sscanf(line,"nameserver %63s",candidate)==1){struct in_addr a;if(inet_pton(AF_INET,candidate,&a)==1){strcpy(server,candidate);break;}}}fclose(f);}
 int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return EAI_SYSTEM;struct timeval timeout={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));struct sockaddr_in dst={0};dst.sin_family=AF_INET;dst.sin_port=htons(53);inet_pton(AF_INET,server,&dst.sin_addr);
 if(connect(fd,(struct sockaddr*)&dst,sizeof(dst))||send(fd,q,p,0)!=(ssize_t)p){close(fd);return EAI_AGAIN;}ssize_t received=recv(fd,answer,sizeof(answer),0);close(fd);if(received<12||u16(answer)!=id||!(answer[2]&128)||(answer[3]&15))return EAI_AGAIN;
 size_t n=(size_t)received;unsigned questions=u16(answer+4),records=u16(answer+6);if(questions>16||records>128)return EAI_FAIL;p=12;for(unsigned i=0;i<questions;i++){if(skip(answer,n,&p)||p+4>n)return EAI_FAIL;p+=4;}
 for(unsigned i=0;i<records;i++){if(skip(answer,n,&p)||p+10>n)return EAI_FAIL;unsigned type=u16(answer+p),klass=u16(answer+p+2),len=u16(answer+p+8);p+=10;if(p+len>n)return EAI_FAIL;if(type==1&&klass==1&&len==4){memcpy(ip,answer+p,4);return 0;}p+=len;}return EAI_NONAME;
}
int getaddrinfo(const char *host,const char *service,const struct addrinfo *hints,struct addrinfo **result){
 *result=NULL;if(hints&&hints->ai_family!=AF_UNSPEC&&hints->ai_family!=AF_INET)return EAI_FAMILY;
 unsigned long port=0;if(service){char*end;port=strtoul(service,&end,10);if(*end){if(!strcmp(service,"https"))port=443;else if(!strcmp(service,"http"))port=80;else return EAI_SERVICE;}if(port>65535)return EAI_SERVICE;}
 struct in_addr ip;int rc=0;if(host){if(hints&&(hints->ai_flags&AI_NUMERICHOST)&&inet_pton(AF_INET,host,&ip)!=1)return EAI_NONAME;rc=lookup(host,&ip);}else ip.s_addr=htonl(hints&&(hints->ai_flags&AI_PASSIVE)?INADDR_ANY:INADDR_LOOPBACK);if(rc)return rc;
 struct addrinfo *ai=calloc(1,sizeof(*ai)+sizeof(struct sockaddr_in));if(!ai)return EAI_MEMORY;struct sockaddr_in*address=(struct sockaddr_in*)(ai+1);address->sin_family=AF_INET;address->sin_port=htons((uint16_t)port);address->sin_addr=ip;ai->ai_family=AF_INET;ai->ai_socktype=hints&&hints->ai_socktype?hints->ai_socktype:SOCK_STREAM;ai->ai_protocol=hints?hints->ai_protocol:0;ai->ai_addrlen=sizeof(*address);ai->ai_addr=(struct sockaddr*)address;if(hints&&(hints->ai_flags&AI_CANONNAME)&&host){ai->ai_canonname=strdup(host);if(!ai->ai_canonname){free(ai);return EAI_MEMORY;}}*result=ai;return 0;
}
void freeaddrinfo(struct addrinfo*ai){while(ai){struct addrinfo*next=ai->ai_next;free(ai->ai_canonname);free(ai);ai=next;}}
int getnameinfo(const struct sockaddr*sa,socklen_t length,char*host,socklen_t hostlen,char*service,socklen_t servicelen,int flags){(void)flags;if(sa->sa_family!=AF_INET||length<sizeof(struct sockaddr_in))return EAI_FAMILY;const struct sockaddr_in*a=(const struct sockaddr_in*)sa;if(host&&hostlen&&!inet_ntop(AF_INET,&a->sin_addr,host,hostlen))return EAI_OVERFLOW;if(service&&servicelen){int n=snprintf(service,servicelen,"%u",ntohs(a->sin_port));if(n<0||(unsigned)n>=servicelen)return EAI_OVERFLOW;}return 0;}
