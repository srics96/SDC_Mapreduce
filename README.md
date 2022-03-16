# MapReduce C++ Base Repository
## Installation
The mapreduce images are prebuilt and the pods can be started through kubernetes configuration file.
kubectl apply -f k8_config.yaml

zookeeper can be installed through
helm repo add bitnami https://charts.bitnami.com/bitnami
helm install default bitnami/zookeeper
