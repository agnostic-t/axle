#!/usr/bin/bash

set -e

echo "[setup] installing yyjson library"

mkdir /tmp/yyjson_setup
cd /tmp/yyjson_setup

echo "[setup][yyjson] cloning repo"
git clone https://github.com/ibireme/yyjson.git

cd yyjson

echo "[setup][yyjson] starting cmake preparation"
cmake -E make_directory build; cd build

cmake ..

echo "[setup][yyjson] starting to build library"
cmake --build .

echo "[setup][yyjson] installing library"
sudo cp ./*.a /usr/local/lib/
sudo cp ../src/*.h /usr/local/include/

echo "[setup][yyjson] done"
