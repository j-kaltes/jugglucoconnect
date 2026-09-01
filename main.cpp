#include <iostream>
#include <string_view>
#include <unistd.h>
#include <string> 
#include <signal.h>
  #include <sys/resource.h>

#include "logs.hpp"
extern void startwatchthread(int port);
extern int sslport;
extern std::string_view globalbasedir;

std::string startsslwatchwithoutthread();
void startSSLonly();
extern bool useSSL;
int main(int argc,char **argv) {
   struct rlimit rl;
   rl.rlim_cur = 1024;
   rl.rlim_max = 1024;
   if (setrlimit(RLIMIT_NPROC, &rl) < 0) {
       LOGGER("setrlimit(RLIMIT_NPROC failed %s\n",strerror(errno));
       }
    else {
       LOGAR("setrlimit(RLIMIT_NPROC) succeed");
       }
   if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
       LOGGER("setrlimit(RLIMIT_NOFILE failed %s\n",strerror(errno));
       }
    else {
       LOGAR("setrlimit(RLIMIT_NOFILE) succeed");
       }
    signal(SIGPIPE, SIG_IGN);
    if(argc>1)
        sslport=atoi(argv[1]);
    else
        sslport=6789;
     LOGGERALL("start %s, port=%d\n",argv[0],sslport);
    while(true) {
         auto error=startsslwatchwithoutthread();
         // NOLOG intentionally suppresses request metadata, but operators must
         // still be able to see that the TLS listener is unavailable.
         loggert(error.empty()
            ? "TLS listener stopped; retrying\n"
            : "TLS listener unavailable; retrying\n");
         sleep(2);
        }
    }
