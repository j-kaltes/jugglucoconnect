/*      This file is part of Juggluco, an Android app to receive and display         */
/*      glucose values from Freestyle Libre 2 and 3 sensors.                         */
/*                                                                                   */
/*      Copyright (C) 2021 Jaap Korthals Altes <jaapkorthalsaltes@gmail.com>         */
/*                                                                                   */
/*      Juggluco is free software: you can redistribute it and/or modify             */
/*      it under the terms of the GNU General Public License as published            */
/*      by the Free Software Foundation, either version 3 of the License, or         */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      Juggluco is distributed in the hope that it will be useful, but              */
/*      WITHOUT ANY WARRANTY; without even the implied warranty of                   */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                         */
/*      See the GNU General Public License for more details.                         */
/*                                                                                   */
/*      You should have received a copy of the GNU General Public License            */
/*      along with Juggluco. If not, see <https://www.gnu.org/licenses/>.            */
/*                                                                                   */
/*      Fri Jan 27 12:38:28 CET 2023                                                 */
#define USE_SSL 1

#include <assert.h>
#include <charconv>
#include <type_traits>
#include <inttypes.h>
#include <system_error>
#include <utility>
#include <alloca.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifndef HAVE_NOPRCTL
#include <sys/prctl.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/syscall.h> 
//#include <map>
#include <chrono>
#include <atomic>
#include <memory>
//#include <latch>

#include "logs.hpp"
#include "netstuff.hpp"
#include "destruct.hpp"

#include "jugglucoconnect.hpp"

//#include "datestring.hpp"

#include "common.hpp"

#include "inout.hpp"
#define LOGERROR(form,...) LOGGER(form ": %s\n",__VA_ARGS__ ,strerror(errno))
#define LOGARERROR(...) LOGGER(__VA_ARGS__  ": %s\n",strerror(errno))
using namespace std::chrono_literals;

typedef std::conditional<sizeof(long long) == sizeof(int64_t), long long, int64_t >::type longlongtype;

//extern jugglucotext engtext;
using namespace std::literals;

static void webserverloop(int *sockptr,bool secure) ;
static bool startwebserver(bool secure,int port,int *sockptr) {
   static constexpr const char serverbuf[]="SSL Juggluco Con";
    const char *servername= serverbuf+(secure?0:4);
   LOGGER("%s\n",servername);
   constexpr const int maxport=20;
   char webserverport[maxport];
   snprintf(webserverport,maxport,"%d",port);

#ifndef HAVE_NOPRCTL
        prctl(PR_SET_NAME, servername, 0, 0, 0);
#endif
   struct addrinfo hints{.ai_flags=AI_PASSIVE,.ai_family=AF_INET6,.ai_socktype=SOCK_STREAM};
   int sock;
   {
   struct addrinfo *servinfo=nullptr;
   destruct serv([&servinfo]{ if(servinfo)freeaddrinfo(servinfo);});
   if(int status= getaddrinfo(nullptr,webserverport,&hints,&servinfo)) {
      LOGGER("getaddrinfo: %s\n",gai_strerror(status));
      return false;
      }
   for(struct addrinfo *ips=servinfo;;ips=ips->ai_next) {
      if(!ips) {
         return false;
         }
      sock=socket(AF_INET6,ips->ai_socktype,ips->ai_protocol);
      if(sock==-1) {
         LOGARERROR("socket");
         continue;
         }
//         exchange_owner_tag(sock,0,reinterpret_cast<uint64_t>(sockptr));
      const int  yes=1;   
      if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
         LOGARERROR("setsockopt");
         close(sock);
        //close_with_tag(sock, reinterpret_cast<uint64_t>(sockptr));
         return false;
         }
      if(bind(sock,ips->ai_addr,ips->ai_addrlen)==-1) {
         LOGERROR("bind port=%d",port);
         close(sock);
        // close_with_tag(sock, reinterpret_cast<uint64_t>(sockptr));
         continue;
         }
      break;
      }
   }
   constexpr int const BACKLOG=SOMAXCONN;
   if (listen(sock, BACKLOG) == -1) {
      LOGARERROR("listen");
      close(sock);
      //close_with_tag(sock, reinterpret_cast<uint64_t>(sockptr));
      return false;
      }
   *sockptr=sock;
   webserverloop(sockptr,secure) ;
   return true;
   }

#include <thread>
#include <algorithm>

#define _GNU_SOURCE 1
#include <sched.h>

static int xdripserversock=-1;
static int xdripserversslsock=-1;

static bool stopconnection=false;

#ifdef USE_SSL
extern bool sslstopconnection;

void stopsslwatchthread() ;
std::string startsslwatchthread() ;

const std::string initsslserver(void);
int sslport=7878;
//std::string testkeyfiles() ;
std::string startsslwatchthread() {

extern std::string haskeyfiles() ;
   auto error=haskeyfiles();
   if(error.size())
      return error;
      
   extern   std::string  loadsslfunctions() ; 
   static std::string keepworking=loadsslfunctions() ;
   if(keepworking.size()) {
      LOGGERN(keepworking.data(),keepworking.size());
      return keepworking;
      }
   auto working=initsslserver();
   if(working.size()==0) {
      std::thread watchsec(startwebserver,true,sslport,&xdripserversslsock);
      watchsec.detach();
      sslstopconnection=false;
      }
   else {
        LOGGER("**** %s ****\n",working.data());
      }
   return working;
   }

std::string startsslwatchwithoutthread() {
extern std::string haskeyfiles() ;
   auto error=haskeyfiles();
   if(error.size())
      return error;
      
   extern   std::string  loadsslfunctions() ; 
   static std::string keepworking=loadsslfunctions() ;
   if(keepworking.size()) {
      LOGGERN(keepworking.data(),keepworking.size());
      return keepworking;
      }
   auto working=initsslserver();
   if(working.size()==0) {
      startwebserver(true,sslport,&xdripserversslsock);
      sslstopconnection=false;
      }
   else {
        LOGGER("**** %s ****\n",working.data());
      }
   return working;
   }
void stopsslwatchthread() {
   sslstopconnection=true;
   shutdown(xdripserversslsock,SHUT_RDWR);
   }
#endif

static std::string sha1secret;
bool useSSL=true;
extern void startsummarythread();
void startwatchthread(int port) {
   if(xdripserversock==-1)  {
      std::thread watch(startwebserver,false,port,&xdripserversock);
      watch.detach();
   #ifdef USE_SSL
   if(xdripserversslsock==-1)  {
      if(useSSL) {
         const std::string error=startsslwatchthread();
         if(error.size()) {
         #ifdef JUGGLUCO_APP
            LOGGER("%s\n",error.data());
        #else
            fprintf(stderr,"*** %s ***\n",error.data());
        #endif
            }
         }
      }
   #endif
      }
   stopconnection=false;
   }
void startSSLonly() {
   #ifdef USE_SSL
   if(xdripserversslsock==-1)  {
      if(useSSL) {
         const std::string error=startsslwatchthread();
         if(error.size()) {
         #ifdef JUGGLUCO_APP
            LOGGER("%s\n",error.data());
        #else
            fprintf(stderr,"*** %s ***\n",error.data());
        #endif
            }
         }
     stopconnection=false;
      }
   #endif
   }
extern void stopwatchthread() ;
void stopwatchthread() {
   stopconnection=true;
   shutdown(xdripserversock,SHUT_RDWR);
#ifdef USE_SSL
   stopsslwatchthread();
#endif
   }

//&hosts[hostlen-1]

static void webserverloop(int *sockptr,bool secure)  {
   int serversock=*sockptr;
   while(true) {  // main accept() loop
       struct sockaddr_in6 their_addr;
//      struct sockaddr_storage their_addr;

      struct sockaddr *addrptr= (struct sockaddr *)&their_addr;
      socklen_t sin_size = sizeof(their_addr) ;
      LOGGER("accept(%d,%p,%d)\n",serversock,addrptr,sin_size);
      int new_fd = accept(serversock, addrptr, &sin_size);
      LOGGER("na accept(serversock)=%d\n",new_fd);
      if (new_fd == -1) {
         int ern=errno;
         flerror("accept %d",ern);
         switch(ern) {
            case EFAULT: 
            case EPROTO:
            case EBADF:
            case EINVAL:
            case ENOTSOCK:
            case EOPNOTSUPP: 
            if(*sockptr==serversock)
               *sockptr=-1;
            close(serversock);
            //close_with_tag(serversock, reinterpret_cast<uint64_t>(sockptr));
            LOGAR("exit"); return;
            } 
         continue;
         }
      const namehost name(addrptr);
      const char * namestr=name;
     LOGGER("%swebserver: got connection from %s sock=%d\n" ,secure?"secure":"",namestr ,new_fd);
      void handlewatch(int sock,const namehost) ;
      try {
          if(secure) {
            void handlewatchsecure(int sock,const namehost) ;
             sslstopconnection=false;
             std::thread  handlecon(handlewatchsecure,new_fd,name);
              handlecon.detach();
             }
          else 
          {
             stopconnection=false;
             std::thread  handlecon(handlewatch,new_fd,name);
             handlecon.detach();
             }
          }
      catch (const std::exception& e)     {
           LOGGER("exception %s close(%d)\n",e.what(),new_fd );
           close(new_fd);
          }
      catch (...)     {
         LOGGER("exception close(%d)\n",new_fd);
         close(new_fd);
        }
      }
   }
static bool   plainwatchcommands(int sock,const char*);
extern void sendtimeout(int sock,int secs);
extern void receivetimeout(int sock,int secs) ;

void handlewatch(int sock,const namehost name) {
try {
     const char threadname[]="ConnectThread";
     LOGGER("handlewatch %d\n",sock);
#ifndef HAVE_NOPRCTL
     prctl(PR_SET_NAME, threadname, 0, 0, 0);
#endif
     //receivetimeout(sock,60);
     //sendtimeout(sock,5*60);
     plainwatchcommands(sock,name.data());
     close(sock);
     }
     catch (const std::exception& e)     {
           LOGGER("handlewatch exception %s close(%d)\n",e.what(),sock );
           close(sock);
           }
    catch(...) {
           LOGGER("handlewatch exception close(%d)\n",sock );
           close(sock);
        }
     }

std::string_view servererrorstr="HTTP/1.0 500 Internal Server Error\r\n\r\n";

void servererror(int sock) {
   send(sock,servererrorstr.data(),servererrorstr.size(),0);
   }



static bool    sgvinterpret(const char *start,int len,bool headonly, bool gmt,std::string_view origin,recdata *data,bool all=true) ;


static bool givetreatments(const char *args,int argslen, std::string_view origin,recdata *data) ;


static bool sendall(int sock ,const char *buf,int buflen) {
        int itlen,left=buflen;
        LOGGER("sock=%d sendall len=%d\n",sock,buflen);
        for(const char *it=buf;(itlen=send(sock,it,left,0))<left;) {
          int waser=errno;
          LOGGER("len=%d\n",itlen);
          if(itlen<0) {
               errno=waser;
               flerror("send(%d,%p,%d)",sock,it,left);
               if(waser==EINTR)
                  continue;
               return false;
               }
          it+=itlen;
          left-=itlen;
          }
        LOGAR("success sendall");
        return true;
        }

#include "valid_check.hpp"

struct raw_check:public valid_check {
    int sockfd;
    raw_check(int sock):sockfd(sock) {}
    virtual bool valid() const override {
      unblock un(sockfd);
      if(!un.unblocked) {
        return true;
        }
      char ch;
      int len=recv(sockfd,&ch,1,MSG_PEEK);
      if(len>0) {
            LOGAR("raw_check: could read: valid");
            return true;
            }
       if(len==0) {
         LOGAR("raw_check: closed");
         return false;
         }
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            LOGAR("raw_check: wants  read/write");
            return true;
            }
       LOGAR("raw_check: could not read: non valid");
      return false;
      }
 } ;
bool watchcommands(char *rbuf,int len,recdata *outdata,bool secure,valid_check &check,const char *host) ;
static bool plainwatchcommands(int sock,const char *host) {
   constexpr const int RBUFSIZE=4096;
   char rbuf[RBUFSIZE];
   int len;
   if((len=recvni(sock,rbuf,RBUFSIZE))==-1) {
      servererror(sock);
      return false;
      }
   if(len==0) {
      LOGAR("shutdown");
      return false;
      }
   struct recdata outdata;

   if(stopconnection)
      return false;
   if(writeall(
#ifdef __ANDROID_API__
   "/data/local/tmp/web/input.dat"
#else
   "/tmp/input.dat"
#endif
   ,rbuf,len)) {
      LOGGER("write succeeded\n");
      }
   else
      LOGGER("write failed\n");

   raw_check check(sock);
   bool res=watchcommands(rbuf, len,&outdata,false,check,host); 
   bool res2=sendall( sock ,outdata.data(),outdata.size()) ;
   LOGGER("plainwatchcommands: delete outdata.allbuf=%p\n",outdata.allbuf);
   delete[] outdata.allbuf;
   outdata.allbuf=nullptr;
   return res&&res2&&!stopconnection;
   }


static bool alloworigin(std::string_view origin) {
       return true;
   }

static bool mkhtml(recdata *outdata,std::string_view origin,std::string_view header,std::string_view bodyhtml,bool dark=false) {
    static constexpr const char startresponse[]="HTTP/1.1 200 OK";
    static constexpr const char allowheader[]="\r\nAccess-Control-Allow-Origin: ";
    static constexpr const char contentlength[]="\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: ";
    static   constexpr const char seperator[]="\r\n\r\n"; 

    static   constexpr const char prehead[]=R"(<!DOCTYPE html>
    <html>
    <head>
    )";

    static   constexpr const char prebody[]=R"(</head><body>)";
    static   constexpr const char predarkbody[]=R"(</head><body style="background-color:black;color:white">)";
    static   constexpr const char htmlend[]=R"(
    </body>
    </html>
    )";


    const bool allow=alloworigin(origin);
    int weblen=sizeof(prehead)+(dark?sizeof(predarkbody):sizeof(prebody))+sizeof(htmlend)-3+header.size()+bodyhtml.size();
    int totlen=weblen+sizeof(startresponse)+sizeof(seperator)+5+sizeof(contentlength)-1+(allow?(sizeof(allowheader)-1+origin.size()):0);
    char *start=outdata->allbuf=new(std::nothrow) char[totlen];
    if(!start) {
            LOGGER("mkhtml new[%d] failed\n",totlen);
            return false;
            }
    char *endptr=start;
    addar(endptr, startresponse);
    if(allow) {
        addar(endptr,allowheader);
        addstrview(endptr,origin);
        }
    addar(endptr,contentlength);
    endptr+=sprintf(endptr,"%d",weblen);
    addar(endptr,seperator);
    #ifndef NOLOG
    const char *startpage=endptr;
    #endif
    addar(endptr,prehead);
    addstrview(endptr,header);
    if(dark) {
       addar(endptr,predarkbody);
       }
    else {
        addar(endptr,prebody);
        }
    addstrview(endptr,bodyhtml);
    addar(endptr,htmlend);
    outdata->start=start;
    outdata->len=endptr-start;
    LOGGER("mkhtml predict=%d pagelen=%ld totlen=%d reallen=%d\n",weblen,endptr-startpage,totlen,outdata->len);
    assert((endptr-startpage)==weblen);
    return true;
}
void mktypeheader(char *outstart,char *outiter,const bool headonly,recdata *outdata,std::string_view type,std::string_view origin) ;
 void mkjsonheader(char *outstart,char *outiter,const bool headonly,recdata *outdata,std::string_view origin) ;
static bool givestatushtml(recdata *outdata) {
static   constexpr const char statusstart[]="HTTP/1.1 200 OK\r\nX-Powered-By: Express\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET,PUT,POST,DELETE,OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type, Authorization, Content-Length, X-Requested-With\r\nVary: Accept, Accept-Encoding\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 18\r\nDate: ";

constexpr const char	daylabel[][4]{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr const char	monthlabel[][4]={
      "Jan",
      "Feb",
      "Mar",
      "Apr",
      "May"      ,             
      "Jun",
       "Jul",
       "Aug",
       "Sep",
      "Oct",
      "Nov",
      "Dec"};
static   constexpr const char statusend[]=" GMT\r\nConnection: keep-alive\r\nKeep-Alive: timeout=5\r\n\r\n<h1>STATUS OK</h1>";

   constexpr const int totlen=sizeof(statusstart)+sizeof(statusend)+60;
   char *status=outdata->allbuf=new(std::nothrow) char[totlen];
   constexpr int startlen=sizeof(statusstart)-1;
   memcpy(status,statusstart,startlen);
   char *ptr=status+startlen;
   auto nu=time(nullptr);
   struct tm stmbuf;
   gmtime_r(&nu,&stmbuf);
   ptr+=sprintf(ptr,R"(%s, %02d %s %04d %02d:%02d:%02d)",
daylabel[stmbuf.tm_wday],stmbuf.tm_mday, monthlabel[stmbuf.tm_mon],stmbuf.tm_year+1900,
stmbuf.tm_hour,stmbuf.tm_mday,stmbuf.tm_sec);
   const int statusendlen=sizeof(statusend)-1;
   memcpy(ptr,statusend,statusendlen);
   
   outdata->start=status;
   outdata->len=ptr-status+statusendlen;
   return true;
}
static bool outofmemory(recdata *outdata) {

static   constexpr const char status[]="HTTP/1.1 413 Content Too Large\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 30\r\n\r\n<h1>Server out of memory</h1>\n";
   const int statuslen=sizeof(status)-1;
   outdata->allbuf=nullptr;
   outdata->start=status;
   outdata->len=statuslen;
   return true;
}
static bool toolarge(recdata *outdata) {
static   constexpr const char status[]="HTTP/1.1 413 Content Too Large\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 36\r\n\r\n<h1>Request will take too long</h1>\n";
   const int statuslen=sizeof(status)-1;
   outdata->allbuf=nullptr;
   outdata->start=status;
   outdata->len=statuslen;
   return true;
}


static bool giveservererror(recdata *outdata) {
   outdata->allbuf=nullptr;
   outdata->len=servererrorstr.size();
   outdata->start=servererrorstr.data();
   return true;
   }
/*Disadvantage: more tokens
Advantage: gets new token earlier in case Juggluco restarted. Juggluco doesn't save tokens
*/


//static bool mkhtml(recdata *outdata,std::string_view header,std::string_view bodyhtml) ;
class AddHost {
    static   constexpr const char slashes[]=R"(://)";
    const std::string_view hostname;
    const bool secure;
    public:
    AddHost(std::string_view hostname,bool secure): hostname(hostname),secure(secure) {
        }
    size_t size() const {
        return hostname.size()+secure+sizeof(slashes)-1;
        }
    char *add(char *bufptr) const {
        if(secure)
            *bufptr++='s';
         addar(bufptr,slashes);
         addstrview(bufptr,hostname) ;
        return bufptr; 
        }
    };
static bool givesite(recdata *outdata,std::string_view hostname,bool secure) {
    static constexpr const char refresh[]{ R"(<meta http-equiv="refresh" content="0; url=https://www.juggluco.nl">)"};
    return mkhtml(outdata,"*"sv,{refresh,sizeof(refresh)-1},""sv);
   }


void mkjsonheader(char *outstart,char *outiter,const bool headonly,recdata *outdata,std::string_view origin)  {
   mktypeheader(outstart,outiter,headonly,outdata, "application/json; charset=utf-8",origin);
   }
bool givenothing(recdata *outdata) {
   LOGAR("givenothing");
   const std::string_view nothing="{}\n";
   outdata->allbuf=new(std::nothrow) char[nothing.size()+512];
   if(!outdata->allbuf) {
      return outofmemory(outdata);
      }
   char *start=outdata->allbuf+152;
   memcpy(start,nothing.data(),nothing.size());
   mkjsonheader(start,start+nothing.size(),false,outdata,"*");
   return true;
}

void wrongpath(std::string_view toget, recdata *outdata) {
   const char notfoundtxt[]="HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\nContent-Length: ";
   constexpr const int startlen=sizeof(notfoundtxt)-1;
   constexpr int maxant=4096;
   char *notfound=outdata->allbuf=new char[maxant];
   memcpy(notfound,notfoundtxt,startlen);
   const char notpath[]="Bad Request: ";
   constexpr const int notpathlen=sizeof(notpath)-1;
   int pathlen=std::find(toget.begin(),toget.end(),' ')-toget.begin();
   constexpr const int maxpath=(maxant-startlen-30);
   if(pathlen>maxpath)
      pathlen=maxpath;
   int reslen=notpathlen+pathlen;
   char *iter= notfound+startlen;
   iter+=sprintf(iter,"%dr\n\r\n",reslen);
   memcpy(iter,notpath,notpathlen);
   iter+=notpathlen;
   memcpy(iter,toget.data(),pathlen);
   iter[pathlen]='\0';
   outdata->len=iter+pathlen-notfound;
   outdata->start=notfound;
   }


static bool putdescription(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *host);
static bool getdescription(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *host);
static bool getaddress(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *host);
static bool putaddress(const char *input,int inputlen,std::string_view origin,recdata *outdata,const char *host);
static bool putdone(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *host) ;
static bool putfailure(const char *input,int inputlen,std::string_view origin,recdata *outdata,const char *host) ;
bool watchcommands(char *rbuf,int len,recdata *outdata,bool secure,valid_check &check,const char *host) {
   LOGGER("watchcommands len=%d %.*s\n",len,len,rbuf);
   const char *start=rbuf;
   const char *ends=rbuf+len;
   const char *nl;
   std::string_view toget;
   bool beput=false;
   bool json=false;
   const char reget[]= "GET /";
   const int regetlen=sizeof(reget)-1;
   const char reput[]= "PUT /";
   const int reputlen=sizeof(reput)-1;
   int length=0;
   std::string_view hostname,origin,referer;
   while((nl= std::find(start,ends,'\n'))!=ends) {
      if(!memcmp(start,reget,regetlen)) {
         const char *reststart=start+regetlen;
         toget={reststart,(std::string_view::size_type)(nl-reststart)};
         }
      else {
         if(!memcmp(start,reput,reputlen)) {
            const char *reststart=start+reputlen;
            toget={reststart,(std::string_view::size_type)(nl-reststart)};
            beput=true;
            }
         else {
              constexpr const char lengthstr[]{R"(Content-Length: )"};
              constexpr const int lengthlen=sizeof(lengthstr)-1;
              if(!memcmp(start,lengthstr,lengthlen)) {
                  sscanf(start+lengthlen,"%d",&length);
                  }
              else { 
                   constexpr const char jsonstr[]=R"(Accept: application/json)";
                   if(!memcmp(start,jsonstr,sizeof(jsonstr)-1)) {
                      json=true;
                      LOGAR("Accepts json");
                      }
                   else {
                      {
                      constexpr const char originnamestr[]="Origin: ";
                      constexpr const int originnamelen= sizeof(originnamestr)-1;
                      if(!memcmp(start,originnamestr,originnamelen)) {
                         const char *name=start+originnamelen;
                         origin={name,static_cast<size_t>(nl-name-(nl[-1]==0x0D?1:0))};
                         LOGGER("Origin=%.*s\n",(int)origin.size(),origin.data());
                         }
                      else
                          {
                         constexpr const char hostnamestr[]="Host: ";
                         constexpr const int hostnamelen= sizeof(hostnamestr)-1;
                         if(!memcmp(start,hostnamestr,hostnamelen)) {
                            const char *name=start+hostnamelen;
                               
                            hostname={name,static_cast<size_t>(nl-name-(nl[-1]==0x0D?1:0))};
                            }
                           }
                           }
                      }
                  }
            }
         }
      start=nl+1;
      if(*start==0xD||*start=='\n')
         break;
      }
   
   if(!toget.data()) {
      LOGGERALL("%s: empty connect\n",host);
      givenothing(outdata);
      return false;
      }
   if(!toget.size()||*toget.data()==' '||*toget.data()=='?') {
      LOGGERALL("%s: no path\n",host);
      givesite(outdata,hostname,secure);
      return true;
      }
  // LOGGER("toget=%.*s\n",(int)toget.size(),toget.data()); //to set getargs in the beginning and use everywhere
    constexpr const char description[]="description";
    constexpr const int descriptionlen=sizeof(description)-1;
    if(!memcmp(description,toget.data(),descriptionlen)) {
            return (beput?putdescription:getdescription)(ends-length,length,origin,outdata,check,host);
        }
    constexpr const char address[]="address";
    constexpr const int addresslen=sizeof(address)-1;
    if(!memcmp(address,toget.data(),addresslen)) {
        if(beput)
            return putaddress(ends-length,length,origin,outdata,host);
         else
            return getaddress(ends-length,length,origin,outdata,check,host);
        }
    constexpr const char done[]="done";
    constexpr const int donelen=sizeof(done)-1;
    if(!memcmp(done,toget.data(),donelen)) {
        if(beput)
            return putdone(ends-length,length,origin,outdata,check,host);
        }
    constexpr const char failure[]="failure";
    constexpr const int failurelen=sizeof(failure)-1;
    if(!memcmp(failure,toget.data(),failurelen)) {
        if(beput)
            return putfailure(ends-length,length,origin,outdata,host);
        }
   LOGGERALL("%s: wrong path %.*s\n",host,toget.size(),toget.data());
   wrongpath(toget,outdata);
   return true;
   }

void mktypeheader(char *outstart,char *outiter,const bool headonly,recdata *outdata,std::string_view type,std::string_view origin)  {
   static constexpr const char httpok[]="HTTP/1.1 200 OK";
   static constexpr const char allowheader[]="\r\nAccess-Control-Allow-Origin: ";
   static constexpr const char contenttype[]="\r\nContent-Type: ";

   const bool allow=alloworigin(origin);
   const int header1len=allow
      ?  (sizeof(httpok)+sizeof(allowheader)+sizeof(contenttype)+origin.size()-3)
      :  (sizeof(httpok)+sizeof(contenttype)-2);
   constexpr const char content[]="\r\nContent-Length: " ;
   constexpr const int contentlen=sizeof(content)-1;
   const int headerstartlen=header1len+contentlen+type.size();
   int uitlen=outiter-outstart;
   constexpr const int maxlen=20;
   char lenstr[maxlen];
   const int getlen=snprintf(lenstr,maxlen,"%d\r\n\r\n",uitlen);
   const int headerlen=headerstartlen+getlen;
   char * const startheader=outstart-headerlen;
   char *ptr=startheader;
   addar(ptr,httpok);
   if(allow) {
      addar(ptr,allowheader);
      addstrview(ptr,origin) ;
      }
   addar(ptr,contenttype);
   addstrview(ptr,type) ;
   memcpy(ptr,content,contentlen);
   memcpy(startheader+headerstartlen,lenstr,getlen);
   int totlen;
   if(headonly) {
      totlen=headerlen;
      startheader[totlen]='\0';
      }
   else
       totlen=outiter-startheader;
   LOGAR("START:");
   LOGGERN(startheader,std::min(400,totlen));
   outdata->start=startheader;
   outdata->len=totlen;

   }
void sockopt(int new_fd) {

//    LOGGER("sockopt(%d)\n",new_fd);
       const int keepalive = 1;
       if(setsockopt(new_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        flerror("setsockopt(%d,SO_KEEPALIVE, ) failed",new_fd);
         }
      int retalive=-4;
    socklen_t retlen=sizeof(retalive);    

       if(getsockopt(new_fd, SOL_SOCKET, SO_KEEPALIVE, &retalive, &retlen) < 0) {
        flerror("getsockopt(%d,SO_KEEPALIVE, ) failed",new_fd);
         }
//    else LOGGER("KEEPALIVE=%d\n",retalive);
       const int keepcnt = 1;
    if(setsockopt(new_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt))<0) {
        flerror("setsockopt(%d,TCP_KEEPCNT ) failed",new_fd);
        }
    retlen=sizeof(retalive);    
    if(getsockopt(new_fd, IPPROTO_TCP, TCP_KEEPCNT, &retalive, &retlen)<0) {
        flerror("getsockopt(%d,TCP_KEEPCNT ) failed",new_fd);
        }
//       else LOGGER("KEEPCNT=%d\n",retalive);
//       if(setsockopt(new_fd, IPPROTO_TCP, TCP_SYNCNT, keepcnt)<0)  {
 //       flerror("setsockopt(%d,TCP_SYNCNT) failed",new_fd); }
       const int keepidle = 10;
       if(setsockopt(new_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
        flerror("setsockopt(%d,TCP_KEEPIDLE, ) failed",new_fd);
         }
    retlen=sizeof(retalive);    

       if(getsockopt(new_fd, IPPROTO_TCP, TCP_KEEPIDLE, &retalive, &retlen) < 0) {
        flerror("getsockopt(%d,TCP_KEEPIDLE, ) failed",new_fd);
         }
//    else LOGGER("KEEPIDLE=%d\n",retalive);
       const int keepintvl = 10;
       if(setsockopt(new_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
        flerror("setsockopt(%d,TCP_KEEPINTVL, ) failed",new_fd);
         }
    retlen=sizeof(retalive);    
       if(getsockopt(new_fd, IPPROTO_TCP, TCP_KEEPINTVL, &retalive, &retlen) < 0) {
        flerror("getsockopt(%d,TCP_KEEPINTVL, ) failed",new_fd);
         }
//    else LOGGER("KEEPINTVL=%d\n",retalive);
     }
void receivetimeout(int sock,int secs) {
   LOGGER("receivetimeout(%d,%d)\n",sock,secs);
   struct timeval tv;
   tv.tv_usec = 0;
   tv.tv_sec = secs;
   setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
   }
void sendtimeout(int sock,int secs) {
     LOGGER("sendtimeout(%d,%d)\n",sock,secs);
     struct timeval tv;
     tv.tv_usec = 0;
     tv.tv_sec = secs;
     setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO , (const char*)&tv, sizeof tv);

     const int  user_timeout = 94000;
     if(setsockopt(sock, IPPROTO_TCP, TCP_USER_TIMEOUT, &user_timeout, sizeof(user_timeout))) {
          flerror("setsockopt(%d,TCP_USER_TIMEOUT, ) failed",sock);
          }
     int retalive=-7;
     socklen_t retlen=sizeof(retalive);   
     if(getsockopt(sock, IPPROTO_TCP, TCP_USER_TIMEOUT, &retalive, &retlen)) {
          flerror("getsockopt(%d,TCP_USER_TIMEOUT, ) failed",sock);
          }
      else {
         LOGGER("USER_TIMEOUT=%d\n",retalive);
         }
   }

#include <queue>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
struct address_t:public std::string {
public:
    address_t():std::string() {
        }
    address_t(int s, const char *dat) {
        LOGGER("address_t %p\n",this);
        clear();
        append(dat,s);
        }
    };
struct description_t:public address_t {
    std::queue<address_t> addresses;
    std::mutex mutex;
    std::condition_variable condi; 
    std::atomic_bool finished{false};
    description_t():address_t(),finished(false) {
        LOGGER("description_t finished=%d\n",finished.load());
        }
    description_t(int s, const char *dat):address_t(s,dat),finished(false) {
        }
    };
struct Connection_t {
    uint32_t unixtime;
    description_t descriptions[2];
//    std::latch done{2};
//    std::binary_semaphore done
    std::mutex mutex;
    std::condition_variable done; 
    bool finished() const {   
        return descriptions[0].finished.load()&&descriptions[1].finished.load();
        }
    Connection_t() {}
    Connection_t(uint32_t now):unixtime(now){ }
    };
#include <zlib.h>
#include <tbb/concurrent_hash_map.h>
//#include <unordered_map>
#include <array>

constexpr const uLong hashfunc(const char *d, int len) {
    return crc32(0,(const unsigned char*)d,len);
    }
#include  "Agent_data.hpp"
#include "keystring.hpp"
struct KeyStringHash {
    static size_t hash(const keystring& a ) {
        return crc32(0,(const unsigned char*)a.data(),a.size());
    }
    //! True if strings are equal
    static bool equal( const keystring& x, const keystring& y ) {
        return x==y;
    }
};
using ConnectionPtr=std::shared_ptr<Connection_t>;
typedef tbb::concurrent_hash_map<const keystring, ConnectionPtr,KeyStringHash>  BaseMap ;
class Alldata: public BaseMap  {
template <typename Accessor>
bool findEntry(Accessor &a,const std::string_view label)  {
        struct  {
            size_t len;
            const char *buf;
            } key{label.size(),label.data()};
         return BaseMap::find(a,*reinterpret_cast<keystring*>(&key));
        }
template <typename Accessor>
bool findEntry(Accessor &a,const std::string_view label) const  {
        struct  {
            size_t len;
            const char *buf;
            } key{label.size(),label.data()};
         return BaseMap::find(a,*reinterpret_cast<const keystring*>(&key));
        }
public:
/*
template <typename Self>
auto   findEntry(this Self&&self,const std::string_view label)  {
        BaseMap::const_accessor a;  
        if(self.findEntry(a,label)) {
               return &a->second;
               } 
        return end;
        } */
ConnectionPtr findEntry(const std::string_view label) const {
        BaseMap::const_accessor a; 
        if(findEntry(a,label)) {
               return a->second;
               } 
        return {};
        } 

ConnectionPtr findEntry(const Agent_data *agent) const {
      return findEntry(agent->getLabel());
      }
ConnectionPtr getEntry2(const std::string_view label,uint32_t now) const {
     BaseMap::const_accessor search;  
     if(findEntry(search,label))   {
               LOGGER("getEntry(%.16s) success\n",label.data());
                 return search->second;
                }
      else  {
               LOGGER("getEntry(%.16s) failed\n",label.data());
                return {};
                }
    }
    /*
auto *getEntry(const std::string_view label,uint32_t now) const {
    return getEntry2(label,now);
    }*/
ConnectionPtr getEntry(const std::string_view label,uint32_t now) const {
      return getEntry2(label,now);
      }
      /*
template <typename Self>
auto *getEntry(this Self&&self,const Agent_data *agent,uint32_t now)  {
      return self.getEntry(agent->getLabel(),now);
      } */
      /*
auto *getEntry(const Agent_data *agent,uint32_t now) const {
      return getEntry2(agent->getLabel(),now);
      } */
ConnectionPtr getEntry(const Agent_data *agent,uint32_t now) const {
      return getEntry(agent->getLabel(),now);
      }
      /*
auto *getEntry(const Agent_data *agent,uint32_t now) {
      auto *ret=reinterpret_cast<std::add_const<decltype(this)>::type>(this)->getEntry(agent->getLabel(),now );
      return reinterpret_cast<std::remove_const<decltype(ret)>::type>(ret);
      } */
//std::shared_mutex mutex;

ConnectionPtr makeEntry(const std::string_view label,uint32_t now)  {
     BaseMap::accessor a;  
     if(findEntry(a,label))   {
         LOGGER("old Item %s\n",label.data());
         }
      else  {
         emplace(a,label,std::make_shared<Connection_t>(now));
         LOGGER("addItem %s\n",label.data());
         }
//     entry=&a->second;  
     return a->second;
     }
ConnectionPtr makeEntry(const Agent_data *agent,uint32_t now)  {
      return makeEntry(agent->getLabel(),now);
      }
ConnectionPtr putEntry(const std::string_view label,int where,const std::span<const char> description,uint32_t now)  {
      auto entry=makeEntry(label,now);
      if(!entry||!entry->descriptions[where].empty()) {
         eraseEntry(label,entry);
         return nullptr;
         }
      entry->descriptions[where].append(description.data(),description.size());
      LOGGER("putEntry size=%ld\n",description.size());
      return entry;
      };
ConnectionPtr putEntry(const Agent_data *agent,const int datalen,uint32_t now)  {
//      return putEntry(agent->getLabel(),agent->getWhere(),{agent->getDescription(),(size_t)datalen},now);
      return putEntry(agent->getLabel(),agent->getWhere(),agent->getDescription(),now);
      }


bool eraseEntry(const std::string_view label,const ConnectionPtr &expected) {
    int res;
    {
//    std::lock_guard<std::shared_mutex> lck(mutex);
    BaseMap::accessor a;  
    if(find(a,label))  {
        if(a->second!=expected) {
            LOGGER("Keep newer %.16s\n",label.data());
            return false;
            }
        res= eraseOnly(a);
        LOGGER("Erased %.16s=%d\n",label.data(),res);
        }
    else {
        LOGGER("Already erased %.16s\n",label.data());
        res=false;
        }
    }; 
    return res;
    }
/*
constexpr bool erase(iterator it)  {
     LOGAR("erase iterator");
     {
     std::lock_guard<std::mutex> lck(mutex);
     ret=eraseOnly(it);
     };
     sleep(1);
     return ret;
     }*/
     private:
bool eraseOnly(BaseMap::accessor &a)  {
    LOGAR("eraseOnly");
    ConnectionPtr addr=a->second;
    addr->descriptions[0].finished=true;
    addr->descriptions[1].finished=true;
    addr->descriptions[0].condi.notify_all();
    addr->descriptions[1].condi.notify_all();
    addr->done.notify_all();
     BaseMap::erase(a);
     return true;
     }
   };
Alldata alldata;
struct BackDescription {
    uint32_t was;
    char description[];
    };

static void makeBackdescription(const address_t &descr,uint32_t oldtime,std::string_view origin,recdata *outdata) {
   const int deslen=descr.size();
   const char *descript=descr.data();
   LOGGER("makeBackdescription %.*s\n",deslen,descript,deslen);
    constexpr const std::string_view plain="application/octet-stream"sv;
    constexpr const int startpos=152;
    const int datalen=deslen+sizeof(BackDescription)+1;
    char *startall=outdata->allbuf=new(std::nothrow) char[startpos+datalen];
    if(!startall) {
        outofmemory(outdata);
        return;
        }
    char *datastart= startall+startpos;
    BackDescription *back=reinterpret_cast<BackDescription*>(datastart);
    back->was=oldtime;
    memcpy(back->description,descript,deslen);
    back->description[deslen]='\0';
    mktypeheader(datastart,datastart+datalen,false,outdata,plain,origin);
    }
    /*
int fd_valid(int sockfd) {
    int oldfl = fcntl(sockfd, F_GETFL);
    if(oldfl<0) {
        LOGGER("fcntl(sockfd, F_GETFL) failed %d\n",oldfl);
        return true;
        }
    if(!fcntl(sockfd, F_SETFL, O_NONBLOCK)) {
       char buf[1];
       int len=read(sockfd,buf,1);
       fcntl(sockfd, F_SETFL, oldfl);
        if(len>=0) {
            LOGAR("could read->valid");
            return true;
            }
       LOGAR("could not read->non valid");
      return false;
      }
   LOGAR("Setting to NONBLOCK failed");
  return true;
}  */
static bool putdescription(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *name) {
    if(inputlen<(sizeof(Agent_data)+20)) {
         LOGGERALL("%s: putdescription(%.*s,%d) too small\n",name,inputlen,input,inputlen);
         wrongpath({input,(size_t)inputlen},outdata);
          return true;
        }
    const int datalen=inputlen-sizeof(Agent_data);
    const Agent_data *agent=reinterpret_cast<const Agent_data *>(input);
    const int here= agent->getWhere();
    if(agent->getLabel().size()<10) {
        LOGGERALL("%s: putdescription label.size()=%d, side=%d totalsize=%d\n",name,agent->getLabel().size(),agent->getWhere(),agent->getDescription().size());
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
        }
    LOGGERALL("%s: putdescription label=%s, side=%d \n",name,agent->getLabel().data(),agent->getWhere(),agent->getDescription().size());
    LOGGER("%.*s\n",agent->getDescription().size(),agent->getDescription().data());
    uint32_t now=time(nullptr);
    auto addr=alldata.putEntry(agent,datalen,now);
    if(!addr) {
        LOGGERALL("%s: end putdescription description not present label=%s side=%d\n",name,agent->getLabel().data(),here);
        return givenothing(outdata);
        }
    uint32_t oldtime=addr->unixtime;
    addr->unixtime=now;
    const int other=!here;
    auto &back=addr->descriptions[other];
    {std::unique_lock<std::mutex> lck(addr->descriptions[other].mutex);
     addr->descriptions[other].condi.notify_all();
     }
    if(here==0) {
        LOGGER("%d putdescription before lock size=%d\n",here,back.size());
        bool invalid=false;
        bool changed=false;
        for(int i=0;i<60;++i) {
            constexpr const int secs=60;
            LOGGER("%d putdescription before wait_for(%d) size=%d\n",here,secs,back.size());
            {
            {
            std::unique_lock<std::mutex> lck(addr->descriptions[here].mutex);
            addr->descriptions[here].condi.wait_for(lck,  std::chrono::seconds(secs), [&back,&check,&changed,&invalid,addr,agent] {return ( (changed=alldata.findEntry(agent)!=addr)|| back.size()>0||(invalid=!check.valid() ||addr->finished()));});   }
            }
            if(changed) {
                 LOGGER("%d label=%s putdescription changed\n",here,agent->getLabel().data());
                  break;
                  }
            if(back.size()>0) {
                LOGGERALL("%s: end putdescription label=%s, side=%d success\n",name,agent->getLabel().data(),agent->getWhere(),agent->getDescription().size());
                makeBackdescription(back, oldtime,origin,outdata);
                return true;
                }
            if(invalid) {
                 LOGGER("putdescription label=%s side=%d not valid\n",agent->getLabel().data(),here);
                  break;
                  }
            uint32_t newtime=time(nullptr);
            if((newtime-oldtime)>10*60) {
                    LOGGER("%d putdescription label=%s waits to long\n",here, agent->getLabel().data());
                    break;
                    }
            }
         }
     else {
         if(back.size()>0) {
            LOGGERALL("%s end putdescription label=%s side=%d success 2\n",name,agent->getLabel().data(),here);
             makeBackdescription(back, oldtime,origin,outdata);
             return true;
             }
         else {
            const bool res=alldata.eraseEntry(agent->getLabel(),addr);
            LOGGERALL("%s end putdescription label=%s side=%d ERROR no data eraseEntry()=%d\\n",name,agent->getLabel().data(),here,res);
            return givenothing(outdata);
            }
         }
    const bool res=alldata.eraseEntry(agent->getLabel(),addr);
    LOGGERALL("%s end putdescription label=%s side=%d eraseEntry()=%d\n",name,agent->getLabel().data(),here,res);
    return givenothing(outdata);
    }
static bool getdescription(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *name) {
    if(inputlen<(sizeof(Agent_data))) {
         LOGGERALL("%s: getdescription(%.*s,%d) too small: ERROR\n" ,name,inputlen,input,inputlen);
         return givenothing(outdata);
        }
    const Agent_data *agent=reinterpret_cast<const Agent_data *>(input);
    const int here= agent->getWhere();
    if(agent->getLabel().size()<10) {
        LOGGERALL("%s: getdescription label.size()=%d, side=%d totalsize=%d\n",name,agent->getLabel().size(),agent->getWhere(),
            agent->getDescription().size());
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
        }
    uint32_t wastime=time(nullptr);
    auto addr=alldata.makeEntry(agent,wastime);
    if(!addr) {
        LOGGERALL("%s: getdescription label=%s, side=%d addr==NULL: ERROR\n",name,agent->getLabel().data(),agent->getWhere());
         return givenothing(outdata);
        }
    LOGGERALL("%s: getdescription label=%s, side=%d \n",name,agent->getLabel().data(),agent->getWhere());
    auto &deshere=addr->descriptions[here];
    if(deshere.size()>0) {
        const bool res=alldata.eraseEntry(agent->getLabel(),addr);
        LOGGERALL("%s: getdescription label=%s side=%d eraseEntry()=%d already present\n",name,agent->getLabel().data(),here,res);
        return givenothing(outdata);
        }
    int other=!here;
    bool invalid=false;
    bool changed=false;
    if(addr->descriptions[other].size()>0) {
        auto &back=addr->descriptions[other];
        LOGGERALL("%s: end getdescription() %s side=%d \n",name,agent->getLabel().data(),here);
        uint32_t oldtime=addr->unixtime;
        makeBackdescription(back, oldtime,origin,outdata);
        return true;
        }
    for(int i=0;i<10;++i) {
        LOGGER("getdescription %s side=%d before wait_for\n",agent->getLabel().data(),here);
        {std::unique_lock<std::mutex> lck(addr->descriptions[here].mutex);
        addr->descriptions[here].condi.wait_for(lck,std::chrono::seconds(60),[addr,other,agent,&check,&invalid,&changed] {return 
(changed=(alldata.findEntry(agent)!=addr))||
        addr->descriptions[other].size()>0||(invalid=(!check.valid()||addr->finished()));});   
        }
        if(changed) {
             LOGGER("getdescription %s side=%d changed, stop\n",agent->getLabel().data(),here);
              break;
            }
        if(addr->descriptions[other].size()>0) {
            auto &back=addr->descriptions[other];
            LOGGERALL("%s: end getdescription() %s side=%d \n",name,agent->getLabel().data(),here);
            uint32_t oldtime=addr->unixtime;
            makeBackdescription(back, oldtime,origin,outdata);
            return true;
            }
        else {
            if(invalid) {
                LOGGER("getdescription %s side=%d invalid\n",agent->getLabel().data(),here);
                break;
                }
            uint32_t now=time(nullptr);
            if((now-wastime)>5*60) {
               LOGGER("getdescription(%s) side=%d  waited too long\n",agent->getLabel().data(),here);
                break;
                }
            LOGGER("getdescription(%s) side=%d addr->descriptions[other].size()<=0 wait longer\n",agent->getLabel().data(),here);
            }
        }
    const bool res=alldata.eraseEntry(agent->getLabel(),addr);
    LOGGERALL("%s: end getdescription %s side=%d  eraseEntry()=%d nothing happens\n",name,agent->getLabel().data(),here, res);
    return givenothing(outdata);
    }
static bool putaddress(const char *input,int inputlen,std::string_view origin,recdata *outdata,const char *name) {
    if(inputlen<(sizeof(Agent_data)+20)) {
         LOGGER("putaddress(%.*s,%d) too small\n",inputlen,input,inputlen);
         wrongpath({input,(size_t)inputlen},outdata);
          return true;
        }
    const int datalen=inputlen-sizeof(Agent_data);
    const Agent_data *agent=reinterpret_cast<const Agent_data *>(input);
    if(agent->getLabel().size()<10) {
        LOGGERALL("%s: putaddress label.size()=%d, side=%d \n",name,agent->getLabel().size(),agent->getWhere());
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
        }
    const char *address=agent->getDescription().data();
    const int addresslen=agent->getDescription().size();
    uint32_t now=time(nullptr);
    if(auto addr=alldata.getEntry(agent,now)) {
        auto &desc=addr->descriptions[agent->getWhere()];
        LOGGER("putaddress(%.*s %.*s) side=%d success\n",agent->getLabel().size(),agent->getLabel().data(),addresslen,address,agent->getWhere());
        desc.addresses.emplace(addresslen,address);
        std::lock_guard<std::mutex> lck(desc.mutex);
        desc.condi.notify_all();
        return givenothing(outdata);
        }
    else {
         LOGGER("putaddress(%.*s %.*s) side=%d failed\n",agent->getLabel().size(),agent->getLabel().data(),addresslen,address,agent->getWhere());
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
         }
    }

static bool putdone(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *name) {
    if(inputlen<(sizeof(Agent_data)+20)) {
         LOGGERALL("%s: putdone(%.*s,%d) too small\n",name,inputlen,input,inputlen);
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
        }
    const Agent_data *agent=reinterpret_cast<const Agent_data *>(input);
    bool here=agent->getWhere();
    if(agent->getLabel().size()<10) {
        LOGGERALL("%s: putdone label.size()=%d, side=%d \n",name,agent->getLabel().size(),here);
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
        }
    if(auto found=alldata.findEntry(agent)) {
        Connection_t &addr=*found;
        LOGGER("putdone label=%s side=%d wait_for()\n",agent->getLabel().data(),here);
        auto &deshere=addr.descriptions[here];
        const auto description=agent->getDescription();
        if(deshere.size()!=description.size()||
            memcmp(deshere.data(),description.data(),description.size())) {
            LOGGERN(deshere.data(),deshere.size());
            LOGGERN(agent->getDescription().data(),agent->getDescription().size());
            LOGGERALL("%s: side=%d %s end putdone: stale description ignored\n",name,here,agent->getLabel().data());
            return givenothing(outdata);
            }
        bool finished=false;
        uint32_t wastime=time(nullptr);
        { 
        std::lock_guard<std::mutex> lck(addr.mutex);
        addr.descriptions[here].finished=true;
        addr.done.notify_all();
        }
        for(int i=0;i<10;++i) {
           { std::unique_lock<std::mutex> lck(addr.mutex);
            addr.done.wait_for(lck,std::chrono::seconds(60),[&addr,found,agent,&check,&finished] {return (finished=(

alldata.findEntry(agent)!=found||

            addr.finished()||!check.valid()));});   
            }
            if(!finished) {
                uint32_t now=time(nullptr);
                if((now-wastime)<3*60) {
                        LOGGER("%d putdone(%.*s,%d) wait longer\n",here,inputlen,input,inputlen);
                        continue;
                        }
                }
            bool res=alldata.eraseEntry(agent->getLabel(),found);
            LOGGERALL("%s: putdone() %s side=%d erased %p res=%d\n",name,agent->getLabel().data(),here,&addr,res);
            return givenothing(outdata);
             }
         }
      else {
        LOGGERALL("%s: end putdone() %d %s not longer present\n",name,here,agent->getLabel().data());
        }
    return givenothing(outdata);
    }

static bool putfailure(const char *input,int inputlen,std::string_view origin,recdata *outdata,const char *name)  {
    if(inputlen<(sizeof(Agent_data))) {
         LOGGERALL("%s: putfailure(%.*s,%d) too small\n",name,inputlen,input,inputlen);
        return givenothing(outdata);
        }
    const Agent_data *agent=reinterpret_cast<const Agent_data *>(input);
    bool here=agent->getWhere();
    if(agent->getLabel().size()<10) {
        LOGGERALL("%s: putfailure label.size()=%d, side=%d \n",name,agent->getLabel().size(),here);
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
        }
    auto label=agent->getLabel();
    if(auto iter=alldata.findEntry(agent)) {
        Connection_t &addr=*iter;
        LOGGER("%d putfailure(%.*s) wait_for()\n",here,label.size(),label.data());
        auto &deshere=addr.descriptions[here];
        const auto description=agent->getDescription();
        if(deshere.size()!=description.size()||
            memcmp(deshere.data(),description.data(),description.size())) {
            LOGGERALL("%s: putfailure %s side=%d  description different %s != %.*s\n",name,agent->getLabel().data(),here,deshere.data(),agent->getDescription().size(),agent->getDescription().data());
            }
        else {
            bool res=alldata.eraseEntry(agent->getLabel(),iter);
            LOGGERALL("%s: %d putfailure erase %s = %d\n",name,here,agent->getLabel().data(),res);
            }
        }
     else {
        LOGGERALL("%s: %d putfailure not present %s \n",name,here,agent->getLabel().data());
        }
    return givenothing(outdata);
    }

static bool getaddress(const char *input,int inputlen,std::string_view origin,recdata *outdata,valid_check &check,const char *name) {
    if(inputlen<sizeof(Agent_data )) {
        LOGGER("getaddress(%.*s,%d) too small\n",inputlen,input,inputlen);
        wrongpath({input,(size_t)inputlen},outdata);
        return true;
        }
    const Agent_data *agent=reinterpret_cast<const Agent_data *>(input);
    if(agent->getLabel().size()<10) {
        LOGGERALL("%s: getaddress label.size()=%d, side=%d \n",name,agent->getLabel().size(),agent->getWhere(),agent->getDescription().size());
         wrongpath({input,(size_t)inputlen},outdata);
         return true;
        }
    uint32_t now=time(nullptr);
    LOGGER("getaddress(%d,%.*s)\n",agent->getWhere(),agent->getLabel().size(),agent->getLabel().data());
    if(auto addr=alldata.getEntry(agent,now)) {
        auto &desc=addr->descriptions[!agent->getWhere()];
        auto &addresses=desc.addresses;
        LOGGER("%d: addresses.size()=%d\n", agent->getWhere(),addresses.size());
        bool invalid=false;
        bool changed=false;
        for(int i=0;i<10;++i) {
           {
            std::unique_lock<std::mutex> lck(desc.mutex);
            desc.condi.wait_for(lck, std::chrono::seconds(60), [&addresses,addr,&check,&invalid,agent,&changed] {
                if((changed=alldata.findEntry(agent)!=addr))
                    return true; 
                bool fin= addr->finished();
                LOGGER("getaddress %s side=%d fin=%d\n",agent->getLabel().data(),agent->getWhere(),fin);
                return !addresses.empty()|| (invalid=(fin||!check.valid())) ;});   
            }
            if(changed) {
                LOGGER("getaddress finished %d\n",agent->getWhere());
                return givenothing(outdata);
                }
            if(!addresses.empty()) { 
                uint32_t oldtime=addr->unixtime;
                makeBackdescription(addresses.front(), oldtime,origin,outdata);
                addresses.pop();
                return true;
                }
             else {
                if(invalid) {
                    LOGAR("getaddress not valid");
                    break;
                    }
                uint32_t newnow=time(nullptr);
                if((newnow-now)>60*3) {
                        LOGGER("getaddress waited too long %d \n",agent->getWhere());
                        return givenothing(outdata);
                        }
                LOGGER("getaddress wait longer %d\n",agent->getWhere());
                }

            }
        const bool res=alldata.eraseEntry(agent->getLabel(),addr);
        LOGGER("getaddress(%.*s) side=%d eraseEntry()=%d\n",agent->getLabel().size(),agent->getLabel().data(),agent->getWhere(),res);
        wrongpath({input,(size_t)inputlen},outdata);
        return true;
        }
     else {
        LOGGER("getaddress(%.*s) side=%d getEntry failed failed\n",agent->getLabel().size(),agent->getLabel().data(),agent->getWhere());
            
        }
    return givenothing(outdata);
    }
