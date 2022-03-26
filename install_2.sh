#!/usr/bin/env bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
numprocs=$(cat /proc/cpuinfo | grep processor | wc -l)

echo "Installing Casablanca"
sudo apt-get install -y g++ git libboost-all-dev libwebsocketpp-dev openssl libssl-dev ninja-build libxml2-dev uuid-dev libunittest++-dev
mkdir -p src
cd ~/src
git clone https://github.com/Microsoft/cpprestsdk.git casablanca
cd casablanca
mkdir build.release
cd build.release
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
ninja
cd Release/Binaries
./test_runner *_test.so
cd -
sudo ninja install
sudo ldconfig

echo "Creating the CMake Find file"
cd /tmp
wget https://raw.githubusercontent.com/Azure/azure-storage-cpp/master/Microsoft.WindowsAzure.Storage/cmake/Modules/LibFindMacros.cmake
sudo mv LibFindMacros.cmake /usr/local/share/cmake-3.19/Modules
sudo wget https://raw.githubusercontent.com/Azure/azure-storage-cpp/master/Microsoft.WindowsAzure.Storage/cmake/Modules/FindCasablanca.cmake
sudo mv FindCasablanca.cmake /usr/local/share/cmake-3.19/Modules
wget https://raw.githubusercontent.com/Tokutek/mongo/master/cmake/FindSSL.cmake
sudo mv FindSSL.cmake /usr/local/share/cmake-3.19/Modules

echo "Install Azure Storage CPP"
cd ~/src
git clone https://github.com/Azure/azure-storage-cpp.git
cd azure-storage-cpp/Microsoft.WindowsAzure.Storage
mkdir build.release
cd build.release
#CASABLANCA_DIR=/usr/local/lib CXX=g++-7 cmake .. -DCMAKE_BUILD_TYPE=Release
CASABLANCA_DIR=/usr/local/lib CXX=g++-7 cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/g++
make -j${numprocs}
#Install library
sudo cp ~/src/azure-storage-cpp/Microsoft.WindowsAzure.Storage/build.release/Binaries/*  /usr/local/lib
sudo rm /usr/local/lib/libazurestorage.so 
sudo rm /usr/local/lib/libazurestorage.so.7
sudo ln -s /usr/local/lib/libazurestorage.so.7.5 /usr/local/lib/libazurestorage.so
sudo ln -s /usr/local/lib/libazurestorage.so.7.5 /usr/local/lib/libazurestorage.so.7
sudo cp -r ~/src/azure-storage-cpp/Microsoft.WindowsAzure.Storage/includes/* /usr/local/include

sudo ldconfig

#Create the CMake Find File
cd ${DIR}
sudo cp FindAzureStorageCpp.cmake /usr/local/share/cmake-3.19/Modules
