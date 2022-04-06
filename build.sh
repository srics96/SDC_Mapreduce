# Script to build your protobuf, c++ binaries, and docker images here
cmake .
make -j
docker build -f docker/Dockerfile.worker -t worker_image .
docker build -f docker/Dockerfile.master -t master_image .
az acr login -n mapreduceimage
docker tag worker_image:latest mapreduceimage.azurecr.io/mapreduce/worker_image
docker push mapreduceimage.azurecr.io/mapreduce/worker_image

docker tag master_image:latest mapreduceimage.azurecr.io/mapreduce/master_image
docker push mapreduceimage.azurecr.io/mapreduce/master_image