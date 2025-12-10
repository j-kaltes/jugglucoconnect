OPT=-O3 -DLOGMUCH
#OPT= -DLOGMUCH
CXXFLAGS=-std=c++26  $(OPT) -g -pthread #-static
CFLAGS=-g  $(OPT) -pthread
LDFLAGS= $(OPT) -g #-static
LOADLIBES=-lssl -lcrypto -ldl  -lz -ltbb
jugglucoconnect: main.o sslserver.o jugglucoconnect.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LOADLIBES) 

clean:
	rm main.o sslserver.o jugglucoconnect.o


%.i:%.cpp
	$(CXX) $(CXXFLAGS) -E $^ -o $@ 

