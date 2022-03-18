#include <zookeeper/zookeeper.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <chrono>
#include <thread>

#include <iostream>
#include <memory>
#include <string>

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

int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_stderrthreshold = 0;
    LOG(INFO) << "The Master has started" << endl;

    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    std::unique_ptr<ConservatorFramework> framework = factory.newClient("my-release-zookeeper:2181");
    framework->start();

    LOG(INFO) << "Connected to the zookeeper service" << endl;
    
    string serverAddress = "worker-service:5001";
    LOG(INFO) << "The server address is " << serverAddress << endl;
    MasterClient masterClient(grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials()));
    LOG(INFO) << "Connected to server" << endl;
    string message(serverAddress);
    string reply = masterClient.handshake(message); 
    LOG(INFO) << "Handshake response received: " << reply << std::endl;
    return 0;
}