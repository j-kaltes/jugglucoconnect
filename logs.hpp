#pragma once

#ifndef LOGGERALL
#include <time.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#undef _GNU_SOURCE
#define _GNU_SOURCE 1
#ifndef INCLUDE_NR
    #define INCLUDE_NR
    #include <asm-generic/unistd.h> /*Headers in this order*/
    #include <sys/syscall.h>
#endif // INCLUDE_NR
#include <unistd.h>
inline  int mkstartlog(char *buf,const int maxbuf) {
    auto tid=(long)syscall(SYS_gettid);
    if(tid==-1)
        tid=getpid();
    struct timeval tv;
    gettimeofday(&tv,nullptr);
    return snprintf(buf,maxbuf,"%lu.%03d %ld ",tv.tv_sec,(int)(tv.tv_usec/1000), tid);
    }

inline  int loggert(const char* fmt, ... )  {
    const int maxbuf=160;
    char buf[maxbuf];
    int start=mkstartlog(buf,maxbuf);
    va_list args;
    va_start(args, fmt);
    start+=vsnprintf(buf+start,maxbuf-start, fmt, args);
    va_end(args);
    write(STDERR_FILENO,buf,start);
    return start;
    }
#define LOGGERALL loggert
#define LOGARALL(...) LOGGERALL("%s\n",__VA_ARGS__)
class Logstart {
private:
  static  constexpr const int maxbuf=55;
    char buf[maxbuf];
    const int len=mkstartlog(buf,maxbuf);
public:
 const   char *data() const {
        return buf;
        }
    size_t size() const {
        return len;
        }
    };
inline void LOGGERALLN(const char *ar,size_t n) { 
        static constexpr const char nl[]{"\n"};
        Logstart start;
        const struct iovec  out[3]{{(void*)start.data(),start.size()},{(void*)ar,n},{(void*)nl,sizeof(nl)-1}};
        writev(STDERR_FILENO,out,3);
        };
#define lerror(x) flerror("%s\n",x)

inline void flerror(const char* fmt, ...){
    int waser=errno;
    const int maxbuf=160;
    char buf[maxbuf];
    size_t start=mkstartlog(buf,maxbuf);
    va_list args;
    va_start(args, fmt);
    start+=vsnprintf(buf+start,maxbuf-start, fmt, args);
    va_end(args);
    static constexpr const char nl[]{"\n"};
    char *error=strerror(waser);
    size_t erlen=strlen(error);
    const struct iovec  out[3]{{(void*)buf,start},{(void*)error,erlen},{(void*)nl,sizeof(nl)-1}};
    writev(STDERR_FILENO,out,3);
    }
#ifndef LOGMUCH
#define LOGGER(...)
#define LOGAR(...)
#define LOGGERN(...)
#else
#define LOGGER(...) LOGGERALL(__VA_ARGS__)
#define LOGAR(...) LOGARALL(__VA_ARGS__)
#define LOGGERN(...) LOGGERALLN(__VA_ARGS__)
#endif
#endif
