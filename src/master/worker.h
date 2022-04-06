#include <zookeeper/zookeeper.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include <grpc++/grpc++.h>

#include <iostream>
#include <string>
#include <vector>

using grpc::Channel;
using namespace std;

enum WorkerStatus {idle, busy, down};

class WorkerInstance {
    
    private:
        int id;
        WorkerStatus status;
        std::string address;
        std::shared_ptr<Channel> channel;
        friend class Master;
    
    public:
        WorkerInstance(int id, std::string address, std::shared_ptr<Channel> channel) {
            this->id = id;
            this->address = address;
            this->status = idle;
            this->channel = channel;
        }

        static vector<shared_ptr<WorkerInstance>> populate() {
            
            string serverAddress = node_ip + ":5001";
            auto channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());

            auto worker_instance = make_shared<WorkerInstance>(id, node_ip, channel);
            worker_instances.push_back(worker_instance);
            id = id + 1;
            
            
            
            // ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
            // unique_ptr<ConservatorFramework> framework = factory.newClient("default-zookeeper:2181");
            // framework->start();

            // LOG(INFO) << "Connected to the zookeeper service" << endl;

            // auto res = framework->checkExists()->forPath("/workers");
            // assert(res == ZOK);
            // LOG(INFO) << "/workers now exists";

            // std::vector<std::string> everyone = framework->getChildren()->forPath("/workers");
            
            // int id = 1;
            // vector<shared_ptr<WorkerInstance>> worker_instances;
            // for (auto element: everyone) {
            //     auto node_ip = element.substr(0, element.find('_'));
            //     cout << "Element: " << element << endl;
            //     cout << "Worker Node IP: " << node_ip << endl;

            //     string serverAddress = node_ip + ":5001";
            //     auto channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());

            //     auto worker_instance = make_shared<WorkerInstance>(id, node_ip, channel);
            //     worker_instances.push_back(worker_instance);
            //     id = id + 1;
            // }
            // return worker_instances;
        }

};