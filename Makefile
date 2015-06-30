OBJS= src/*.cxx
CC=$(CXX)
CFLAGS = -I. -Wall -g -std=c++11
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lpthread
LOADLIBES = 

all: correct test

test: src/test.cxx src/*.h
	$(CC) $(CFLAGS) src/test.cxx -o bin/test $(LDFLAGS)

correct: src/test_correct_simple.cxx src/*.h
	$(CC) $(CFLAGS) src/test_correct_simple.cxx -o bin/test_correct_simple $(LDFLAGS)
