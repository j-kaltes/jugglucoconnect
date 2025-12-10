char fullchainfileonly[]="fullchain.pem";
char privatekey[]="privkey.pem";
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


#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <signal.h>
#include <string_view>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#ifndef HAVE_NOPRCTL
#include <sys/prctl.h>
#endif
#include "logs.hpp"
//#include "strconcat.hpp"
#include "inout.hpp"
#include "jugglucoconnect.hpp"
#include "destruct.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef __ANDROID_API__
#include <dlfcn.h>

#include <android/dlext.h>
#define DLSYMS_SSL 1
#endif
#ifdef DLSYMS_SSL
#include "openssl/ssl.h"
#include "openssl/err.h"
static const SSL_METHOD *(*TheMethod)(void);
static int (*SSL_library_initptr)(void)=NULL;
static void (*OPENSSL_add_all_algorithms_noconfptr)(void)=NULL;
static void (*SSL_load_error_stringsptr)(void)=NULL;
static SSL_CTX *(*SSL_CTX_newptr)(const SSL_METHOD *method);
static int (*SSL_CTX_use_certificate_chain_fileptr)(SSL_CTX *ctx, const char *file);
static int (*SSL_CTX_use_PrivateKey_fileptr)(SSL_CTX *ctx, const char *file, int type);
static int (*SSL_CTX_check_private_keyptr)(const SSL_CTX *ctx);
static int (*SSL_acceptptr)(SSL *ssl);
static int (*SSL_readptr)(SSL *ssl, void *buf, int num);
static int (*SSL_writeptr)(SSL *ssl, const void *buf, int num);
static int (*SSL_get_fdptr)(const SSL *ssl);
static void (*SSL_freeptr)(SSL *ssl);
static int (*SSL_shutdownptr)(SSL *ssl);
static SSL *(*SSL_newptr)(SSL_CTX *ctx);
static int (*SSL_set_fdptr)(SSL *ssl, int fd);
static void (*SSL_CTX_freeptr)(SSL_CTX *ctx);
static void (*ERR_print_errors_cbptr)(int (*cb)(const char *str, size_t len, void *u), void *u);
static unsigned long (*ERR_get_errorptr)(void);
static void (*ERR_error_string_nptr)(unsigned long e, char *buf, size_t len);
static int (*SSL_peekptr)(SSL *ssl, void *buf, int num);
static int (*SSL_get_errorptr)(const SSL *ssl, int ret);
static long (*BIO_ctrlptr)(BIO *bp, int cmd, long larg, void *parg);
static BIO *(*SSL_get_rbioptr)(SSL *ssl);
//SSL_get_error
//SSL_get_fd

#else
#include <openssl/ssl.h>
#include <openssl/err.h>
#define TheMethod SSLv23_method
#define SSL_library_initptr SSL_library_init
#define OPENSSL_add_all_algorithms_noconfptr OPENSSL_add_all_algorithms_noconf
#define SSL_load_error_stringsptr SSL_load_error_strings
#define SSL_CTX_newptr SSL_CTX_new
#define SSL_CTX_use_certificate_chain_fileptr SSL_CTX_use_certificate_chain_file
#define SSL_CTX_use_PrivateKey_fileptr SSL_CTX_use_PrivateKey_file
#define SSL_CTX_check_private_keyptr SSL_CTX_check_private_key
#define SSL_acceptptr SSL_accept
#define SSL_readptr SSL_read
#define SSL_writeptr SSL_write
#define SSL_shutdownptr SSL_shutdown
#define SSL_get_fdptr SSL_get_fd
#define SSL_freeptr SSL_free
#define SSL_newptr SSL_new
#define SSL_set_fdptr SSL_set_fd
#define SSL_CTX_freeptr SSL_CTX_free
#define ERR_print_errors_cbptr ERR_print_errors_cb
#define ERR_get_errorptr ERR_get_error
#define ERR_error_string_nptr ERR_error_string_n
#define SSL_get_errorptr SSL_get_error
#define SSL_peekptr SSL_peek
#define SSL_ctrlptr SSL_ctrl

#define BIO_ctrlptr BIO_ctrl
#define SSL_get_rbioptr SSL_get_rbio
#endif

#include "netstuff.hpp"

int logcallback(const char *str, size_t len, void *u) {
#ifndef NOLOG
    const char *format=(const char *)u;
    LOGGER(format,str);
#endif
    return 0;
    }
extern int logcallback(const char *str, size_t len, void *u) ;
void  sslerror(const char *format) {
    ERR_print_errors_cbptr(logcallback,(void*)format);
    }

using namespace std::literals;
extern std::string_view globalbasedir;
std::string_view globalbasedir{"."sv};




static pathconcat chainfilename;
static pathconcat private_file;
static bool getkeynames() {
     chainfilename=pathconcat(globalbasedir,fullchainfileonly);
     private_file=pathconcat(globalbasedir,privatekey);
     LOGAR("getkeynames");
     return true;
    }
std::string haskeyfiles() {
[[maybe_unused]] static auto _hasnames=getkeynames();
 if(access(chainfilename.data(), R_OK)!=0) {
    return std::string(fullchainfileonly)+std::string(" missing");
    }
 if(access(private_file.data(), R_OK)!=0) {
    return std::string(privatekey)+std::string(" missing");

    }
    return "";
    }

extern void *opencrypto();
extern void *openssl();
extern void * dlopener(std::string_view filename,int flags);
std::string loadsslfunctions() {
#ifdef DLSYMS_SSL
   #ifndef  __ANDROID_API__
   char cryptolib[]="libcrypto.so.3";
   void* cryptohandle;
   if(!(cryptohandle=dlopener(cryptolib, RTLD_NOW))&&(cryptolib[12]='\0', !(cryptohandle=dlopener(cryptolib, RTLD_NOW)))) {
         cryptolib[12]='.';
        return  std::string("dlopen==nullptr: ");
        }
   #else
   void* cryptohandle=opencrypto();
   if(!cryptohandle) {
        return  std::string("dlopen==nullptr: ");
        }
   #endif
   #define hgetsym(handle,name) *((void **)&name##ptr)=dlsym(handle, #name)
   #define getsym(name) hgetsym(handle,name)
   #define cryptest(name) if(!(hgetsym(cryptohandle,name))) { dlclose(cryptohandle);return std::string(dlerror());;}
   #define symtest(name) if(!(getsym(name))) { dlclose(handle);dlclose(cryptohandle);return std::string(dlerror());;}
   if(!(hgetsym(cryptohandle,ERR_print_errors_cb))) {
        dlclose(cryptohandle);
        return std::string("hgetsym ERR_print_errors_cb fails");
      }
  cryptest(ERR_get_error); 
  cryptest(ERR_error_string_n);

#ifndef __ANDROID_API__
   char libssl[]="libssl.so.3";
   const char *libname=libssl;
     void *handle;
     if(!(handle=dlopener(libname, RTLD_NOW))&&(libssl[9]='\0',!(handle=dlopener(libname, RTLD_NOW)))) {
         libssl[9]='.';
        return std::string("dlopen==nullptr: ");
        }
#else
     void *handle=openssl();
     if(!handle) {
        return std::string("dlopen==nullptr: ");
        }
#endif
     *((void **)&TheMethod)=dlsym(handle, "TLSv1_2_server_method");
     if(!TheMethod) {
     #ifndef NOLOG
        const char *error=dlerror();
        LOGGER("dlsym(TLSv1_2_server_method): %s\n",error?error:"?");
    #endif
      *((void **)&TheMethod)=dlsym(handle, "SSLv23_method");
         if(!TheMethod) {
            const char *error=dlerror();
            dlclose(handle);
            return std::string("dlsym(SSLv23_method): ")+(error?std::string(error):""s);
            }
      }

   getsym(SSL_library_init);
   getsym(OPENSSL_add_all_algorithms_noconf);
   getsym(SSL_load_error_strings);
   symtest(SSL_get_fd);
    symtest(SSL_peek);

   symtest(SSL_get_error);

   symtest(SSL_CTX_new);
   symtest(SSL_CTX_use_certificate_chain_file);
   symtest(SSL_CTX_use_PrivateKey_file);
   symtest(SSL_CTX_check_private_key);
   symtest(SSL_accept);
   symtest(SSL_read);
   symtest(SSL_write);
   symtest(SSL_get_fd);
   symtest(SSL_free);
   symtest(SSL_shutdown);
   symtest(SSL_new);
   symtest(SSL_set_fd);
   symtest(SSL_CTX_free);
#endif
   return "";
 }


static const char *geterrorstring(int error) {
    switch(error) {
        case 0: return "SSL_ERROR_NONE";
        case 1: return "SSL_ERROR_SSL";
        case 2: return "SSL_ERROR_WANT_READ";
        case 3: return "SSL_ERROR_WANT_WRITE";
        case 4: return "SSL_ERROR_WANT_X509_LOOKUP";
        case 5: return "SSL_ERROR_SYSCALL";
        case 6: return "SSL_ERROR_ZERO_RETURN";
        case 7: return "SSL_ERROR_WANT_CONNECT";
        case 8: return "SSL_ERROR_WANT_ACCEPT";
        case 9: return "SSL_ERROR_WANT_ASYNC";
        case 10: return "SSL_ERROR_WANT_ASYNC_JOB";
        case 11: return "SSL_ERROR_WANT_CLIENT_HELLO_CB";
        default: return "SSL_UNKNOWN_ERROR";
        }
}


extern std::string_view servererrorstr;

void    sslservererror(SSL *ssl) {
    SSL_writeptr(ssl,servererrorstr.data(),servererrorstr.size()) ;
    }
extern bool sslstopconnection;
bool sslstopconnection=false;
/*
static int SSLreadfull(SSL* ssl, char *dataptr,const int buflen) {
    LOGGER("start SSLreadfull %d\n", buflen);
    int n=0;
     for(int res;;n+=res) {
        if(n>=buflen) {
            LOGGER("SSL_read all %d\n",n);
            break;
            }
        res=SSL_read(ssl, dataptr+n,buflen-n);
        if(res>0) {
            LOGGER("SSLread %d\n",res);
            continue;
            }
        int err = SSL_get_error(ssl, res);
        LOGGER("SSL_read Error %d %s\n",err,geterrorstring(err));
        if (err == SSL_ERROR_SSL) {
            unsigned long e = ERR_get_error();
            constexpr const int maxbuf=200;
            char buf[maxbuf];
            ERR_error_string_n(e, buf, maxbuf);
            LOGGER("SSL_read SSL_ERROR %s\n",buf);
            }
        else {
            if(err == SSL_ERROR_SYSCALL) {
                lerror("SSL_read syscall error");
                }
            }
         break;
        }
    LOGGER("end SSLreadfull %d\n",n);
    return n;
    }
static int SSLwritefull(SSL* ssl, const char *dataptr,const int buflen) {
    LOGGER("start SSLwritefull %d",buflen);
    int n=0;
     for(int res;;n+=res) {
        if(n>=buflen) {
            LOGGER("SSL_write all %d\n",buflen);
            break;
            }
        res=SSL_writeptr(ssl, dataptr+n,buflen-n);
        if(res>0) {
            LOGGER("SSLwrite %.*s %d\n",res,dataptr+n,res);
            continue;
            }
        int err = SSL_get_error(ssl, res);
        LOGGER("SSL_write Error %d %s\n",err,geterrorstring(err));
        //int err = SSL_get_error(ssl, res);
        if (err == SSL_ERROR_SSL) {
            unsigned long e = ERR_get_error();
            constexpr const int maxbuf=200;
            char buf[maxbuf];
            ERR_error_string_n(e, buf, maxbuf);
            LOGGER("SSL_read SSL_ERROR %s\n",buf);
            }
        else {
            if(err == SSL_ERROR_SYSCALL) {
                lerror("SSL_write syscall error");
                }
            }
         break;
        }
    LOGGER("end SSLwritefull %d\n",n);
    return n;
    }
*/
#include "valid_check.hpp"

struct ssl_check:public valid_check {
    SSL *ssl;
    ssl_check(const ssl_check &check):ssl(check.ssl) {}
    ssl_check(SSL *ssl):ssl(ssl) {}
    virtual bool valid() const override {
      int sockfd= SSL_get_fdptr(ssl);
      unblock un(sockfd);
      char c;
      int ret = SSL_peekptr(ssl, &c, 1);
      if(ret > 0) {
        LOGGER("SSL_peek=%d\n",ret);
          return true;
        } else {
            int err = SSL_get_errorptr(ssl, ret);
            bool want=(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE);
            if(want) {
                LOGGER("SSL_peek err=%d want%s\n",err,(err == SSL_ERROR_WANT_READ)?" READ":((err == SSL_ERROR_WANT_WRITE)?" WRITE":""));
                return want;
                }
           return false;
           }
       }
  };
bool securewatchcommands(SSL *ssl,const char *host) {
    constexpr const int RBUFSIZE=4096;
    char rbuf[RBUFSIZE];
    int len;
    if((len= SSL_readptr(ssl, rbuf,RBUFSIZE))<=0) {
        sslservererror(ssl);
        return false;
        }
   // LOGGER("securewatchcommands len=%d\n",len);
    struct recdata outdata;
    if(sslstopconnection)
        return false;
    bool watchcommands(char *rbuf,int len,recdata *outdata,bool secure, valid_check &check,const char*) ;
#ifndef NOLOG
    if(writeall(
#ifdef __ANDROID_API__
   "/data/local/tmp/web/input.dat"
#else
   "/tmp/input.dat"
#endif
    ,rbuf,len)) {
    //   LOGGER("write succeeded\n");
       }
    else
       LOGGER("write failed\n");
#endif
    ssl_check check(ssl);
    bool res=watchcommands(rbuf, len,&outdata,true,check,host);
    int res2=  SSL_writeptr(ssl,outdata.data(),outdata.size());
   LOGGER("securewatchcommands: delete outdata.allbuf=%p\n",outdata.allbuf);
    delete[] outdata.allbuf;
    return res&&res2>0&&!sslstopconnection;
    } 

static SSL_CTX *globalctx=nullptr;

bool    securewatchcommands(SSL *ssl,int sock);
extern void sendtimeout(int sock,int secs);
extern void receivetimeout(int sock,int secs) ;

#define BIO_set_closeptr(b,c)      (int)BIO_ctrlptr(b,BIO_CTRL_SET_CLOSE,(c),NULL)
#define BIO_get_fdptr(b,c)         BIO_ctrlptr(b,BIO_C_GET_FD,0,(char *)(c))

extern void sockopt(int new_fd) ;
void handlewatchsecure(int sock,const namehost name) {
try {
    static SSL_CTX *ctx=globalctx;
    if(!ctx)  {
        shutdown(sock,SHUT_RDWR);
        close(sock);
        return;
        }
   const char threadname[]="SSL ConnectThread";
#ifndef HAVE_NOPRCTL
   prctl(PR_SET_NAME, threadname, 0, 0, 0);
#endif
   LOGGER("handlewatchsecure %d\n",sock);
   SSL *ssl=SSL_newptr(ctx);  
    if(!ssl) {
        shutdown(sock,SHUT_RDWR);
        close(sock);
        return;
        }
   destruct _des{[sock,ssl]{
      for(int i=0;i<10;++i) { 
           int outq = 0;
            if(ioctl(sock, TIOCOUTQ, &outq) == 0) {
                LOGGER("SIOCOUTQ = %d bytes\n", outq);
                if(outq==0)
                    break;
                }
            else {
                flerror("ioctl(sock, TIOCOUTQ, &outq)): ");
                break;
                }
           
            usleep(500000);
            }
       for(int i=0;i<5;++i) {
           int res=SSL_shutdownptr(ssl);
           LOGGER("SSL_shutdown=%d\n",res);
           if(res) {
               if(res==-1) {
                    unsigned long err;
                    while ((err = ERR_get_errorptr()) != 0) {
                        char buf[256];
                        ERR_error_string_nptr(err, buf, sizeof(buf));
                        LOGGER("OpenSSL error: %s (0x%lx)\n", buf, err);
                        }
                    }
                 break;
                 }
             usleep(1000*500);
             }
#ifdef BIOCLOSE
      bool sslClose;
      BIO* rbio = SSL_get_rbioptr(ssl);
      if(rbio) {
            BIO_set_closeptr(rbio, BIO_CLOSE);   
            sslClose=true;
           }
       else
            sslClose=false;

      SSL_freeptr(ssl);  
      if(!sslClose) { 
            shutdown(sock,SHUT_RDWR);
            close(sock);
             }

        LOGGER("handlewatchsecure close(%d) rbi fd=%d sslClose=%d\n",sock,sslClose);
#else
        shutdown(sock,SHUT_RDWR);
        close(sock);
        SSL_freeptr(ssl);  
#endif
        }};
//   receivetimeout(sock,5*60);
 //  sendtimeout(sock,5*60);
   sockopt(sock);
   int flag = 1;
   setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
   SSL_set_fdptr(ssl, sock); 
   if(SSL_acceptptr(ssl)<0)   { 
      sslerror("SSL_accept: %s");
      return;
      }
    securewatchcommands(ssl,name.data());
   // LOGAR("below securewatchcommands");
   SSL_writeptr(ssl, "", 0);
   BIO* rbio = SSL_get_rbioptr(ssl);
   int bio_fd = -1;
    if (rbio) BIO_get_fdptr(rbio, &bio_fd);
   LOGGER("before SSL_shutdown rbio fd=%d\n", bio_fd);
//   struct linger l = { .l_onoff = 0, .l_linger = 0 };
 //  setsockopt(sock, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
  /*  struct tcp_info ti;
    socklen_t len = sizeof(ti);
    if(getsockopt(sock, IPPROTO_TCP, TCP_INFO, &ti, &len) == 0) {
        LOGGER("tcpi_state=%u tcpi_retransmits=%u tcpi_rto=%u tcpi_snd_mss=%u tcpi_rtt=%u\n",ti.tcpi_state, ti.tcpi_retransmits, ti.tcpi_rto, ti.tcpi_snd_mss, ti.tcpi_rtt);
        }
   LOGAR("after tcpi_state"); */
    }
     catch (const std::exception& e)     {
           LOGGER("handlewatchsecure exception %s close(%d)\n",e.what(),sock );
           }
    catch(...) {
           LOGGER("handlewatchsecure exception close(%d)\n",sock );
        }
    }

 #include <openssl/err.h>


struct call_data_t{
    std::string_view start;
    std::string back;
    } ;
int geterrorcallback(const char *str, size_t len, void *u) {
    call_data_t *dptr=(call_data_t *)u;
    int startlen =dptr->start.size();
    int totlen=startlen+len+3;
    dptr->back=std::string(totlen,0);
    char *uit=dptr->back.data();
    memcpy(uit,dptr->start.data(),startlen);
    uit+=startlen;
    *uit++=':';
    *uit++=' ';
    memcpy(uit,str,len);
    uit[len]='\0';
    return 0;
    }
std::string geterror(std::string_view start) {
    call_data_t data;
    data.start=start;
    ERR_print_errors_cbptr(geterrorcallback,(void*)&data);
    return data.back;
    }
//static SSL_CTX* 
/*
const char *geterror() {
    if(ERR_reason_error_stringptr)
         return ERR_reason_error_stringptr(ERR_peek_last_errorptr());
    else return "openSSL error";
    } */

const std::string initsslserver(void) {
    LOGAR("initsslserver");
    if(globalctx!=nullptr)
        SSL_CTX_freeptr(globalctx);
    globalctx=nullptr;
 class init{
     public:
        init() {
        #ifdef DLSYMS_SSL
            if(SSL_library_initptr) 
        #endif
                SSL_library_initptr();
        #ifdef DLSYMS_SSL
            if(OPENSSL_add_all_algorithms_noconfptr) 
        #endif
                OPENSSL_add_all_algorithms_noconfptr();
        #ifdef DLSYMS_SSL
            if(SSL_load_error_stringsptr) 
        #endif
                SSL_load_error_stringsptr();  
            } 
        };
 static init _init;

    SSL_CTX *ctx = SSL_CTX_newptr(TheMethod());
    if( ctx == NULL ) {
        return geterror("SSL_CTX_newptr");
        }

    destruct des{[ctx] {SSL_CTX_freeptr(ctx);  }};
    
if(SSL_CTX_use_certificate_chain_fileptr(ctx,chainfilename.data())<=0)  {
    return geterror(chainfilename);
    }
if(SSL_CTX_use_PrivateKey_fileptr(ctx, private_file.data(), SSL_FILETYPE_PEM) <= 0) {
    return geterror(private_file.data());
    }
    if( !SSL_CTX_check_private_keyptr(ctx) ) {
    return geterror("Private key does not match the public certificate");
    }
    des.active=false;
    globalctx=ctx;
    return "";
   }



