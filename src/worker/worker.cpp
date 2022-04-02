#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <boost/filesystem.hpp>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "central.grpc.pb.h"
#include "sharding.h"
#include "../util/constants.h"
#include "../util/blob.h"
#include "python_executor.h"

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


vector<string> split (string s, string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;

    while ((pos_end = s.find (delimiter, pos_start)) != string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
}

class TaskExecutor {
    
    private:
        string getFileContents(ShardFileInfo fileInfo) {
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            return as.get_blob_with_offset(fileInfo.fileName, fileInfo.startOffset, (fileInfo.endOffset - fileInfo.startOffset) + 1);
        }


    public:
        string adjust_shard_boundaries(const FileInfo* file){
            cout<< "Inside Adjust Shard " << endl;

            int start = file->start();
            int end = file->end();
            string fname = file->fname();
            cout<< start << " " <<end <<" " <<fname <<endl;
            
            ShardFileInfo sh;
            sh.startOffset = start; 
            sh.endOffset = end;
            sh.fileName = fname;
            //string data = getFileContents(sh);
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

            string data = getFileContents(sh);
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

            //data = getFileContents(sh);
            cout << "Shard boundaries adjusted" << endl;
            return data;
        }

        void map(const Task* task) {   
            cout << "Map task called" << endl;
            string directory_name = "./intermediates";            
            
            if (!boost::filesystem::exists(directory_name)) {
                boost::filesystem::create_directory(directory_name);
                cout << "Directory created" << endl;
            }
            std::vector<FileInfo> files(task->files().begin(), task->files().end());
            string shard_content = "";
            for (auto file: files) {
                shard_content += adjust_shard_boundaries(&file);
                shard_content += " ";
            }

            string blobname = std::to_string(task->worker_id()) + "_" + std::to_string(task->task_id()) + ".txt";
            string filename = directory_name + "/" + blobname;
            std::ofstream out(filename, std::ios::out);
            out << shard_content;
            out.close();

            string mapper_output_file = directory_name + "/" + "mapper_output_file.txt";
            execute(filename, mapper_output_file, "/code/src/sdc_map_reduce/app/mapper.py", "mapper.py",  O_RDWR|O_CREAT);
            cout<< "USER FUNCTION DONE" <<endl;
            std::ifstream ifs(mapper_output_file);
            std::string content( (std::istreambuf_iterator<char>(ifs) ),(std::istreambuf_iterator<char>()) );
            cout << "map result content" << content << endl;
            const std::string s = "\n";
            const std::string t = "\t";

            std::string::size_type n = 0;
            while ( ( n = content.find( s, n ) ) != std::string::npos )
            {
                content.replace( n, s.size(), t );
                n += t.size();
            }
            cout << "After replace" << endl;
            vector<string> tokens = split(s, "\t");
            int reducer_count = task->num_reducers();
            vector<std::ofstream> file_ofstreams; 
            vector<std::string> output_file_names;
            
            for(int i=0 ; i < reducer_count; i++){
                string directory_name = "./intermediates";            
                string blobname = std::to_string(task->worker_id()) + "_" + std::to_string(task->task_id()) + "_" + std::to_string(i+1) + ".txt";
                string filename = directory_name + "/" + blobname;
                file_ofstreams.emplace_back(std::ofstream {filename.c_str()});
                output_file_names.push_back(filename);
            }
            
            hash<string> hasher;
            for(int i=0; i<tokens.size(); i+=2){
                int id = hasher(tokens[i])%reducer_count;
                string to_output = tokens[i] + "\t" + tokens[i+1] + "\n";
                file_ofstreams[id] << to_output;
            }

            for(int i=0 ; i< reducer_count; i++){
                file_ofstreams[i].close();
            }
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            for(int i=0 ; i< reducer_count; i++){
                as.upload_file(output_file_names[i], output_file_names[i]);
            }
            cout << "Map task complete" << endl;
            
        }

        void reduce(const Task* task) {
            std::vector<FileInfo> files(task->files().begin(), task->files().end());
            cout << "Reduce called" << endl;
            /*
            1. Download the M intermediate files.
            2. Call the Reducer M + 1 times.
            */

            string directory_name = "./reducers";            
            
            if (!boost::filesystem::exists(directory_name)) {
                boost::filesystem::create_directory(directory_name);
                cout << "Directory created" << endl;
            }
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            vector<std::string> reducer_file_names;
            for(int i=0 ; i<files.size(); i++){
                string reducer_file = directory_name + "/" + files[i].fname();
                as.save_blob(files[i].fname(), reducer_file);
                reducer_file_names.push_back(reducer_file);
            }

            string temp_out_file = directory_name + "/" + "temp";

            for(int i=0 ; i<files.size(); i++){
                if(i == 0){
                execute(reducer_file_names[i], temp_out_file, "/code/src/sdc_map_reduce/app/reducer.py", "reducer.py",  O_RDWR|O_CREAT); 
                } else { 
                execute(reducer_file_names[i], temp_out_file, "/code/src/sdc_map_reduce/app/reducer.py", "reducer.py",  O_RDWR|O_APPEND); 
                }
            }

            string final_out_file = directory_name + "/" + "final_" +  to_string(task->task_id()) + ".txt";
            execute(temp_out_file, final_out_file, "/code/src/sdc_map_reduce/app/reducer.py", "reducer.py",  O_RDWR|O_CREAT); 
            as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            as.upload_file(final_out_file, final_out_file);
        }
};


class WorkerServiceImpl final : public WorkerService::Service {
    
    int state;

    Status execute_task(
        ServerContext* context, 
        const Task* task,
        TaskReception* reception 
    ) override {
        string reception_text = "Received Task " + to_string(task->task_id()) + " " + task->task_type();
        cout << reception_text << endl;
        TaskExecutor executor;
        if (task->task_type() == "map")
            executor.map(task);
        else
            executor.reduce(task);   
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