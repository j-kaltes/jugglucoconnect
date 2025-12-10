CXX=aarch64-linux-android21-clang++
CC=aarch64-linux-android21-clang
CXXFLAGS=-std=c++26 -g -pthread -DANDROID_STL=c++_static -Iinclude -I/home/jka/src/android/Glucodata/Common/src/main/cpp/include/
CFLAGS=-g -pthread
LDFLAGS=-static-libstdc++
LOADLIBES=-ldl  -lz -ltbb

SOURCEDIR=..
jugglucoconnect: $(SOURCEDIR)/main.cpp $(SOURCEDIR)/sslserver.cpp $(SOURCEDIR)/jugglucoconnect.cpp $(SOURCEDIR)/cryptopen.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LOADLIBES) 
clean:
	rm webserver
