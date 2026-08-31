#pragma once
#include <string_view>
#include <string.h>
struct keystring {
    size_t buflen;
    char *buf;
   keystring():buflen(0),buf(nullptr) {};
    keystring(std::string_view name): buflen(name.size()),buf(new char[name.size()+1]) {
        memcpy(buf,name.data(),name.size());
        buf[buflen]='\0';
        };
    keystring(const char *name,int len): buflen(len),buf(new char[buflen+1]) {
        memcpy(buf,name,buflen);
        buf[buflen]='\0';
        };
    keystring(const char *name): buflen(strlen(name)),buf(new char[buflen+1]) {
        memcpy(buf,name,buflen);
        buf[buflen]='\0';
        };
    keystring(const keystring &other): keystring(std::string_view(other.buf,other.buflen)) {
        }
    keystring( keystring &&other): buflen(other.buflen),buf(other.buf) {
        other.buflen=0;
        other.buf=nullptr;
        }
    ~keystring( ) {
        delete[] buf;
        }
    char *data() {
        return buf;
        }
    const char *data() const {
        return buf;
        }
    size_t size() const {
        return buflen;
        }
    operator std::string_view() {
        return {buf, buflen};
        }
    bool operator==(const keystring& other) const {
        return buflen == other.buflen && !memcmp(buf,other.buf,buflen);
      }
   bool operator < (const keystring& other) const {
           return memcmp(buf,other.buf,buflen)<0;
          }
      bool operator > (const keystring& other) const {
           return memcmp(buf,other.buf,buflen)>0;
          }
    };
