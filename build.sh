# Script to build your protobuf, c++ binaries, and docker images here
cmake .
make -j
docker build -f docker/Dockerfile.worker -t worker_image .
docker build -f docker/Dockerfile.master -t master_image .
docker tag worker_image:latest srics96/worker_image:latest
docker push srics96/worker_image:latest

docker tag master_image:latest srics96/master_image:latest
docker push srics96/master_image:latest