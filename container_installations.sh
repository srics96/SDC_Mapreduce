#!/bin/bash
numprocs=$(nproc)

#Install CMake
echo "Instaling cmake 3.0+"
mkdir -p ~/src
cd ~/src
sudo apt-get remove -y cmake
wget http://www.cmake.org/files/v3.19/cmake-3.19.5.tar.gz
tar xf cmake-3.19.5.tar.gz
cd cmake-3.19.5
./configure
make -j${numprocs}
sudo checkinstall -y --pkgname cmake
echo "PATH=/usr/local/bin:$PATH" >> ~/.profile
source ~/.profile