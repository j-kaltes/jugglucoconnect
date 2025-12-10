#pragma once
#include <fcntl.h>

struct valid_check {
    virtual bool valid() const=0;
   };
struct unblock {
    int oldfl;
    int sockfd;
    bool unblocked;
    unblock(int sock):sockfd(sock) {
        oldfl = fcntl(sockfd, F_GETFL);
        if(oldfl<0) {
            LOGGER("fcntl(sockfd, F_GETFL) failed %d\n",oldfl);
            unblocked=false;
            return;
            }
        unblocked=!fcntl(sockfd, F_SETFL, oldfl|O_NONBLOCK);
       // LOGGER("unblock = %d\n",unblocked);
        }
  ~unblock() {
      if(unblocked) {
           fcntl(sockfd, F_SETFL, oldfl);
           }
       }
    };

