#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "../proto_code/central.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using mapr::WorkerService;
using mapr::HandShakeRequest;
using mapr::HandShakeReply;

using namespace std;

class WorkerServiceImpl final : public WorkerService::Service {
    
    Status handshake(
        ServerContext* context, 
        const HandShakeRequest* request,
        HandShakeReply* reply
    ) override {
        cout << "Received message " << request->message() << endl;
        string response = request->message() + " gatech";
        reply->set_message(response);
        return Status::OK;
    }
};

void runServer() {
    std::string server_address("0.0.0.0:5001");
    WorkerServiceImpl service;

    // grpc::EnableDefaultHealthCheckService(true);
    // grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    
    server->Wait();
}

int main(int argc, char** argv) {
    runServer();
    return 0;
}