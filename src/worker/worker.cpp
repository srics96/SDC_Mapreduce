#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>

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

using mapr::FileInfo;
using mapr::WorkerService;
using mapr::Task;
using mapr::TaskReception;

using namespace std;


class TaskExecutor {
    
    private:
        string getFileContents(ShardFileInfo fileInfo) {
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            return as.get_blob_with_offset(fileInfo.fileName, fileInfo.startOffset, (fileInfo.endOffset - fileInfo.startOffset) + 1);
        }

    public:
        string adjust_shard_boundaries(const FileInfo* file){
            int start = file->start();
            int end = file->end();
            string fname = file->fname();
            ShardFileInfo sh;
            sh.startOffset = start; 
            sh.endOffset = end;
            sh.fileName = fname;
            string data = getFileContents(sh);
            // cout << "INITIAL DATA" << endl << data << endl;
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
            // cout << "Requested DATA" << endl << data << endl;
            
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
            // cout << "Corrected DATA" << endl << data << endl;

            return data;
        }

        void map(const Task* task) {        
            std::vector<FileInfo> files(task->files().begin(), task->files().end());
            string shard_content = "";
            for (auto file: files) {
                shard_content += adjust_shard_boundaries(&file);
                shard_content += " ";
            }
            cout << "Map called" << endl;
            /*
            1. Write shard content to a file.
            2. Call python Mapper.
            */
            
        }

        void reduce(const Task* task) {
            std::vector<FileInfo> files(task->files().begin(), task->files().end());
            cout << "Reduce called" << endl;
            /*
            1. Download the M intermediate files.
            2. Call the Reducer M + 1 times.
            */
        }
};


class WorkerServiceImpl final : public WorkerService::Service {
    
    Status execute_task(
        ServerContext* context, 
        const Task* task,
        TaskReception* reception 
    ) override {
        string reception_text = "Received Task " + to_string(task->task_id()) + " " + task->task_type();
        cout << reception_text << endl;
        TaskExecutor* taskPtr = new TaskExecutor();
        if (task->task_type() == "map")
            std::thread task_thread(&TaskExecutor::map, taskPtr, task);
        else
            std::thread task_thread(&TaskExecutor::reduce, taskPtr, task);
        reception->set_message(reception_text);
        return Status::OK;
    }

};

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
    LOG(INFO) << "Create /workers retval:" << res;
    
    res = framework->checkExists()->forPath("/workers");
    assert(res == ZOK);
    LOG(INFO) << "/workers now exists";

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

void runServer() {
    std::string server_address("0.0.0.0:5001");
    WorkerServiceImpl service;

    // grpc::EnableDefaultHealthCheckService(true);
    // grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    string hostname = get_local_ip();
    register_with_zoopeeker(hostname);
    
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