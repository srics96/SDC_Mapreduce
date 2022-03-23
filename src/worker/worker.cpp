#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "central.grpc.pb.h"
#include "sharding.h"
#include "../util/constants.h"
#include "../util/blob.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using mapr::WorkerService;
using mapr::HandShakeRequest;
using mapr::HandShakeReply;

using namespace std;


class Mapper {
    
    private:
        string getFileContents(ShardFileInfo fileInfo) {
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            return as.get_blob_with_offset("test_blob", fileInfo.startOffset, (fileInfo.endOffset - fileInfo.startOffset) + 1);
        }

    public:
        void map() {
            std::map<string, string> outFilePaths;
            vector<shared_ptr<ShardAllocation>> shards = createShardAllocations();
            for (auto shard: shards) {
                string shardContent;
                for (auto file: shard->files) {
                    shardContent += getFileContents(file);
                    shardContent += "\n";
                }
                shardContent += "gatech";
                string blobname = std::to_string(shard->id) + ".txt";
                string filename = "../../intermediates/" + blobname;
                std::ofstream out(filename, std::ios::out);
                out << shardContent;
                out.close();
                outFilePaths[blobname] = filename;
            }
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);

            std::map<string, string>::iterator it;
            for (it=outFilePaths.begin(); it!=outFilePaths.end(); it++)
                as.upload_file(it->second, "mapper_intermediates/" + it->first);
        }
};

class WorkerServiceImpl final : public WorkerService::Service {
    
    Status handshake(
        ServerContext* context, 
        const HandShakeRequest* request,
        HandShakeReply* reply
    ) override {
        cout << "Received message " << request->message() << endl;
        string response = request->message() + " gatech";
        map();
        reply->set_message(response);
        return Status::OK;
    }

    void map() {
        Mapper mapper;
        mapper.map();
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