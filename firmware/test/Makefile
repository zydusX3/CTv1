# Host build of the shared library + unit tests. Usage: make -C firmware/test
CXX ?= g++
CXXFLAGS ?= -std=c++11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
SRC := $(wildcard ../lib/ctv1/src/*.cpp) test_main.cpp

run: test_ctv1
	./test_ctv1

test_ctv1: $(SRC) $(wildcard ../lib/ctv1/src/*.h)
	$(CXX) $(CXXFLAGS) $(SRC) -lm -o $@

clean:
	rm -f test_ctv1

.PHONY: run clean
