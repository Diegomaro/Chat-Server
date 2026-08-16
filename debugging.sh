#!/bin/bash

CXXFLAGS="-std=c++17 \
-Wall \
-Wextra \
-Wpedantic \
-Wconversion \
-Wshadow \
-Wnon-virtual-dtor \
-Wold-style-cast \
-fsanitize=address,undefined \
-fno-omit-frame-pointer \
-g"

g++ $CXXFLAGS src/server/*.cpp -o build/server-debug
g++ $CXXFLAGS src/client/*.cpp -o build/client-debug