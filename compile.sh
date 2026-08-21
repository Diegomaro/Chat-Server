#!/bin/bash
g++ -std=c++17 src/common/*.cpp src/server/*.cpp -o build/server
g++ -std=c++17 src/common/*.cpp src/client/*.cpp -o build/client
