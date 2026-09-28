CXX      := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra

SOL ?= solution

.PHONY: all grade clean

all: build/$(SOL) build/grader

build/%: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -o $@ $<

build/grader: tools/grader.cpp | build
	$(CXX) $(CXXFLAGS) -o $@ $<

build:
	mkdir -p build

# 사용법: make grade [SOL=solution]
grade: build/$(SOL) build/grader
	./build/grader ./build/$(SOL)

clean:
	rm -rf build
