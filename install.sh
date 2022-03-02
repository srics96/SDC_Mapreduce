#!/bin/bash

# This install script is for debian based linux distributions only

#Install CMake

sudo apt-get install -y g++ zookeeper libzookeeper-mt2 zookeeperd zookeeper-bin libzookeeper-mt-dev ant check build-essential autoconf libtool pkg-config checkinstall git zlib1g libssl-dev
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


# Install Kubectl
sudo apt-get update
sudo apt-get install -y apt-transport-https ca-certificates curl
sudo curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
sudo curl -LO "https://dl.k8s.io/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl.sha256"
echo "$(<kubectl.sha256) kubectl" | sha256sum --check
sudo install -o root -g root -m 0755 kubectl /usr/local/bin/kubectl

# Install Protobuf
#sudo apt install -y protobuf-compiler

# Install Kind
export PATH=$PATH:/usr/local/go/bin
go get sigs.k8s.io/kind
export PATH="$PATH:$(go env GOPATH)/bin"

# Install Helm
curl https://baltocdn.com/helm/signing.asc | sudo apt-key add -
sudo apt-get install apt-transport-https --yes
echo "deb https://baltocdn.com/helm/stable/debian/ all main" | sudo tee /etc/apt/sources.list.d/helm-stable-debian.list
sudo apt-get update
sudo apt-get install helm

#Install Conservator


# Install Go GRPC
#go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.26
#go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.1

#Install GRPC


# Setup go module
#go mod init mapreduce
#go mod tidy

# Clean up
sudo rm kubectl
sudo rm kubectl.sha256
