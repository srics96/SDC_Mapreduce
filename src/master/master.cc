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
    string serverAddress = "worker-service:5001";
    cout << "The server address is " << serverAddress << endl;
    MasterClient masterClient(grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials()));
    cout << "Connected to the server.";
    string message(serverAddress);
    string reply = masterClient.handshake(message); 
    cout << "Handshake response received: " << reply << std::endl;
    
    return 0;
}