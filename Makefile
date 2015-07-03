OBJS= src/*.cxx
CC=$(CXX)
CFLAGS = -I. -Wall -std=c++11 -O3
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lpthread
LOADLIBES = 

all: correct correct_M test test_M range range_M

test: src/test.cxx src/*.h
	$(CC) $(CFLAGS) src/test.cxx -o bin/test $(LDFLAGS)
	
test_M: src/test.cxx src/*.h
	$(CC) $(CFLAGS) -D MEM_MANAG src/test.cxx -o bin/testM $(LDFLAGS)

correct: src/test_correct_simple.cxx src/*.h
	$(CC) $(CFLAGS) src/test_correct_simple.cxx -o bin/test_correct_simple $(LDFLAGS)
	
correct_M: src/test_correct_simple.cxx src/*.h
	$(CC) $(CFLAGS) -D MEM_MANAG src/test_correct_simple.cxx -o bin/test_correct_simpleM $(LDFLAGS)

range: src/test_correct_range.cxx src/*.h
	$(CC) $(CFLAGS) src/test_correct_simple.cxx -o bin/test_correct_range $(LDFLAGS)
	
range_M: src/test_correct_range.cxx src/*.h
	$(CC) $(CFLAGS) -D MEM_MANAG src/test_correct_range.cxx -o bin/test_correct_rangeM $(LDFLAGS)
	