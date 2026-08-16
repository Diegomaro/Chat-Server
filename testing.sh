#!/bin/bash

CXXFLAGS="-std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast"

g++ $CXXFLAGS src/server/*.cpp -o build/server
g++ $CXXFLAGS src/client/*.cpp -o build/client