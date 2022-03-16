#include <iostream>
#include <memory>
#include <string>

#include <zookeeper/zookeeper.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <chrono>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "central.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using mapr::WorkerService;
using mapr::HandShakeRequest;
using mapr::HandShakeReply;

using namespace std;


class MasterClient { 
 
    public:
        MasterClient(std::shared_ptr<Channel> channel): stub_(WorkerService::NewStub(channel)) {}

        std::string handshake(string message) {
            HandShakeRequest request;
            request.set_message(message);

            HandShakeReply reply;

            ClientContext context;

            Status status = stub_->handshake(&context, request, &reply);

            if (status.ok()) {
                return reply.message();
            } else {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            }
        
        }
    private:
        std::unique_ptr<WorkerService::Stub> stub_;
};

bool is_master() {
    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    
    std::unique_ptr<ConservatorFramework> framework = factory.newClient("localhost:2181");

    framework->start();
    LOG(INFO) << "Start ZK framework";
    {
        auto res = framework->create()->forPath("/masters");
        LOG(INFO) << "Create /masters retval:" << res;
    }
    {
        auto res = framework->checkExists()->forPath("/masters");
        assert(res == ZOK);
        LOG(INFO) << "/masters now exists";
    }
    std::string realpath;
    {
    auto res = framework->create()
                   ->withFlags(ZOO_EPHEMERAL | ZOO_SEQUENCE)
                   ->forPath("/masters/" + hostname + "_", NULL, realpath);
    if (res != ZOK) {
      LOG(FATAL) << "Failed to create ephemeral node, retval "<< res;
    } else {
      LOG(INFO) << "Created seq ephm node " << realpath;
      realpath = realpath.substr(realpath.find_last_of('/') + 1);
    }
    }
    // Check if we are the primary master
    bool is_main_master = false;
    while (!is_main_master) {
        std::vector<std::string> everyone =
        framework->getChildren()->forPath("/masters");
        auto ref =
        std::min_element(everyone.cbegin(), everyone.cend(),
                         [](const std::string& a, const std::string& b) {
                           return get_trailing_num(a) < get_trailing_num(b);
                         });
        if (*ref == realpath) {
            LOG(INFO) << "This is the master node";
            is_main_master = true;
        } else {
      // TODO set watch
        LOG(INFO) << "Waiting as backup master";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    if (is_main_master) {
    std::string worker;
    std::vector<std::string> workers =
        framework->getChildren()->forPath("/workers");
    if (workers.empty()) {
      LOG(FATAL) << "No workers";
    }
    else {
      worker = workers[0];
    }
        return true;
    }
  while (true) {
    LOG(INFO) << "Waiting to be killed (sleeping for 2 seconds)";
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }
}

int main(int argc, char** argv) {

    string serverAddress = "worker-service:5001";
    cout << "The server address is " << serverAddress << endl;
    is_master();
    MasterClient masterClient(grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials()));
    cout << "Connected to the server.";
    string message(serverAddress);
    string reply = masterClient.handshake(message); 
    cout << "Handshake response received: " << reply << std::endl;
    return 0;
}