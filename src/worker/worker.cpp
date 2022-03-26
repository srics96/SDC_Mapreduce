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

#include <zookeeper/zookeeper.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <glog/logging.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using mapr::WorkerService;
using mapr::HandShakeRequest;
using mapr::HandShakeReply;
using mapr::Task;
using mapr::ShardData;

using namespace std;


class Mapper {
    
    private:
        string getFileContents(ShardFileInfo fileInfo) {
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            return as.get_blob_with_offset("test_blob", fileInfo.startOffset, (fileInfo.endOffset - fileInfo.startOffset) + 1);
        }

    public:
        string adjust_shard_boundaries(const Task* task){
            int start = task->mapshard().start();
            int end = task->mapshard().end();
            string fname = task->mapshard().fname();
            ShardFileInfo sh;
            sh.startOffset = start;
            sh.endOffset = end;
            sh.fileName = fname;
            string data = getFileContents(sh);
            cout << "INITIAL DATA" << endl << data << endl;
            bool first_shard = false;
            if(start == 0){
                sh.startOffset = start;
                first_shard = true;
            } else {
                sh.startOffset = start - 1; 
            }
            
            sh.endOffset = end + 100;
            sh.fileName = fname;

            data = getFileContents(sh);
            cout << "Requested DATA" << endl << data << endl;
            
            int i = 0;
            if(first_shard){

            }
            else if(isalnum(data[i])){
            while(isalnum(data[i])){
                i++;
                }
            while(!isalnum(data[i])){
                i++;
                }
                sh.startOffset += i;
            } else{
                sh.startOffset += 1;
            }

            i = end - start + 1;
            if(isalnum(data[i])){
                while(isalnum(data[i])){
                i++;
                }
                i-=1;
                sh.endOffset = start + i;
            } else{
                sh.endOffset = end;
            }

            data = getFileContents(sh);
            cout << "Corrected DATA" << endl << data << endl;

            return data;
        }

        void new_map(const Task* task) {            
            string shardContent = adjust_shard_boundaries(task);
            shardContent += "\ngatech";
            string blobname = task->taskid() + ".txt";
            string filename = "./intermediates/" + blobname;
            std::ofstream out(filename, std::ios::out);
            out << shardContent;
            out.close();            
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);           
            as.upload_file(filename, "mapper_intermediates/" + blobname);
        }


        void old_map() {
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
                string filename = "./intermediates/" + blobname;
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
        reply->set_message(response);
        return Status::OK;
    }

    Status map(
        ServerContext* context, 
        const Task* task,
        HandShakeReply* reply
    ) override {
        cout << "Received Task " << task->taskid() <<
        " " << task->tasktype() << " " << task->mapshard().fname() 
        << " " <<task->mapshard().start() 
        <<" " << task->mapshard().end() << endl;
        string response = task->taskid() + " gatech";

        string hostname = get_local_ip();
        register_with_zoopeeker(hostname);

        map(task);
        reply->set_message(response);
        return Status::OK;
    }

    string get_local_ip() {
        string ip;
        array<char, 1024> buffer;
        FILE* out = popen("hostname -i", "r");
        assert(out);
        int n_read;
        while ((n_read = fgets(buffer.data(), 512, out) != NULL)) {
            ip += buffer.data();
        }
        pclose(out);
        if (!ip.empty() && ip.back() == '\n') {
            ip.pop_back();
        }
        return ip;
    }

    void register_with_zoopeeker(string local_ip){
        LOG(INFO) << "Elect leader called by " << local_ip << endl;

        ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
        unique_ptr<ConservatorFramework> framework = factory.newClient("default-zookeeper:2181");
        framework->start();

        LOG(INFO) << "Connected to the zookeeper service" << endl;
        
        auto res = framework->create()->forPath("/workers");
        LOG(INFO) << "Create /masters retval:" << res;
        
        res = framework->checkExists()->forPath("/workers");
        assert(res == ZOK);
        LOG(INFO) << "/masters now exists";

        string realpath;
        int mypath;
        
        res = framework->create()->withFlags(ZOO_EPHEMERAL | ZOO_SEQUENCE)->forPath("/workers/" + local_ip + "_", NULL, realpath);
        if (res != ZOK) {
        LOG(FATAL) << "Failed to create ephemeral node, retval "<< res;
        } else {
        LOG(INFO) << "Created seq ephm node " << realpath;
        mypath = stoi(realpath.substr(realpath.find('_') + 1));
        }

    }
    
    void adjust_shard_boundaries(const Task* task){
        Mapper mapper;
        mapper.adjust_shard_boundaries(task);
    }

    void map(const Task* task) {
        Mapper mapper;
        mapper.new_map(task);
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