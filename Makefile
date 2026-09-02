OPT=-O3 -DLOGMUCH
#OPT= -DLOGMUCH
CXXFLAGS=-std=c++20  $(OPT) -g -pthread #-static
CFLAGS=-g  $(OPT) -pthread
LDFLAGS= $(OPT) -g #-static
LOADLIBES=-lssl -lcrypto -ldl  -lz -ltbb
jugglucoconnect: main.o sslserver.o jugglucoconnect.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LOADLIBES) 

stale_generation_test: tests/stale_generation_test.cpp jugglucoconnect.cpp Agent_data.hpp keystring.hpp sslserver.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) tests/stale_generation_test.cpp sslserver.o -o $@ $(LOADLIBES)

test: stale_generation_test
	./stale_generation_test

clean:
	rm -f main.o sslserver.o jugglucoconnect.o jugglucoconnect stale_generation_test


%.i:%.cpp
	$(CXX) $(CXXFLAGS) -E $^ -o $@ 
