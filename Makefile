build-mac:
	g++ --std=c++17 main.cpp

build-linux:
	g++ --std=c++17 -pthread main.cpp

build:
	g++ --std=c++17 main.cpp

run: build
	./a.out

.PHONY: run build build-mac build-linux