# Oneiroi Unit Tests Makefile
# Run: make test

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I..

.PHONY: all test clean

all: test

test: test_commons
	./test_commons

test_commons: test_commons.cpp owl_mocks.h ../ParameterInterpolator.h
	$(CXX) $(CXXFLAGS) test_commons.cpp -o test_commons

clean:
	rm -f test_commons
