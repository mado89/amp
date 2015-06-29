OBJS= src/*.cxx
CC=$(CXX)
CFLAGS = -I. -Wall -O3 -g -std=c++11
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lpthread
LOADLIBES = 

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o bin/test $(LDFLAGS)
