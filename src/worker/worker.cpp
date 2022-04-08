#include <fstream>
#include <iostream>
#include <map>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp> 

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "../generated/central.pb.h"
#include "../generated/central.grpc.pb.h"
#include "sharding.h"
#include "../util/constants.h"
#include "../util/blob.h"
#include "../util/zook.h"
#include "python_executor.h"

#include <zookeeper/zookeeper.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <glog/logging.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;

using mapr::FileInfo;
using mapr::MasterService;
using mapr::WorkerService;
using mapr::ResultFile;
using mapr::Task;
using mapr::TaskCompletion;
using mapr::TaskCompletionAck;
using mapr::TaskReception;

using namespace std;



vector<string> splitter(string s, string delimiter) {
    size_t pos = 0;
    vector<std::string> tokens;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        string token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    return tokens;
}

void update_worker_self_status(string node, string status){
    
    auto zk = ZookeeperHelper();    
    zk.create_if_not_exists(node, status);
    zk.set(node, status);
}


class WorkerClient {

    public:
        WorkerClient(std::string server_address);
        bool sendTaskCompletion(Task* task, vector<string> output_files);

    private:
        std::unique_ptr<MasterService::Stub> stub_;
};

WorkerClient::WorkerClient(std::string server_address)
  : stub_(MasterService::NewStub(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()))) {}

bool WorkerClient::sendTaskCompletion(Task* task, vector<string> output_files) {

    TaskCompletion completion;
    completion.set_worker_id(task->worker_id());
    completion.set_task_id(task->task_id());
   
	for(auto i : output_files){
    	ResultFile* result_file = completion.add_result_files();
    	result_file -> set_filename(i);
    }

    ClientContext context;
	TaskCompletionAck ack;
    
	Status status = stub_->task_complete(&context, completion, &ack);
    if (status.ok()) {
        cout << "Receipt for " << task->task_id() << endl;
        return true;
    }
    else {
        cout << status.error_code() << ": " << status.error_message() << std::endl;
        return false;
    }
    return true;
}

class TaskExecutor {
    
    private:
        queue<Task*> my_tasks;
        friend class DynamicRPCListener;
	    friend class MultiThreadedDispatchServer;
        
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

        vector<string> map(const Task* task) {   
            // cout << "Map task called" << endl;
                
            string job_dir = "./job_" + to_string(task->job_id());
            string directory_name = job_dir + "/intermediates"; 
            
            std::vector<FileInfo> files(task->files().begin(), task->files().end());
            string shard_content = "";
            for (auto file: files) {
                shard_content += adjust_shard_boundaries(&file);
                shard_content += " ";
            }

            string blobname = std::to_string(task->worker_id()) + "_" + std::to_string(task->task_id()) + "_content" + ".txt";
            string filename = directory_name + "/" + blobname;
            std::ofstream out(filename, std::ios::out);
            out << shard_content;
            out.close();

            string mapper_output_file = directory_name + "/" + "mapper_output_file.txt";
            
            string azure_path = "/code/src/app/mapper.py";
            string vm_path = "/vagrant/workshop6-c/src/app/mapper.py";
            
            // cout<< "USER FUNCTION STARTS" <<endl;
            execute(filename, mapper_output_file, azure_path, "mapper.py",  O_RDWR|O_CREAT);
            // cout<< "USER FUNCTION DONE" <<endl;
            std::ifstream ifs(mapper_output_file);
            std::string content( (std::istreambuf_iterator<char>(ifs) ),(std::istreambuf_iterator<char>()) );
            
            const std::string s = "\n";
            const std::string t = " ";

            
            // cout << "After replace" << endl;
            
            std::vector<std::string> lines;
            boost::split(lines, content, boost::is_any_of("\n"), boost::token_compress_on);
            
            vector<string> tokens;
            for (auto line: lines) {
                vector<string> sub_tokens;
                boost::split(sub_tokens, line, boost::is_any_of(" "), boost::token_compress_on);
                
                if (sub_tokens.size() != 0)
                    tokens.push_back(sub_tokens[0]);
            }
            
            int reducer_count = task->num_reducers();
            vector<std::ofstream> file_ofstreams; 
            vector<std::string> output_file_names;
            // cout << "Splitting reduce files" << endl;

            for(int i=0 ; i < reducer_count; i++){
                string blobname = std::to_string(task->worker_id()) + "_" + std::to_string(task->task_id()) + "_" + std::to_string(i+1);
                string filename = directory_name + "/" + blobname;
                file_ofstreams.emplace_back(std::ofstream {filename.c_str()});
                output_file_names.push_back(filename);
            }
            // cout << "File streams created" << endl;
            hash<string> hasher;
            for(int i=0; i<tokens.size(); i+=1){
                int id = hasher(tokens[i])%reducer_count;
                string to_output = tokens[i] + " " + "1" + "\n";
                // cout << to_output << endl;
                file_ofstreams[id] << to_output;
            }
            // cout << "Data appended to streams" << endl;
            for(int i=0 ; i< reducer_count; i++){
                file_ofstreams[i].close();
            }
            // cout << "Splitting completed" << endl;
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            
            for(int i=0 ; i< reducer_count; i++){
                as.upload_file(output_file_names[i], output_file_names[i]);
            }
            cout << "Map task complete" << endl;
            return output_file_names;
        }

        vector<string> reduce(const Task* task) {
            vector<string> output_files;
            std::vector<FileInfo> files(task->files().begin(), task->files().end());
            cout << "Reduce called" << endl;
            /*
            1. Download the M intermediate files.
            2. Call the Reducer M + 1 times.
            */

            string job_dir = "./job_" + to_string(task->job_id());
            string directory_name = job_dir + "/reducers";
            
            auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            vector<std::string> reducer_file_names;
            for(int i=0 ; i<files.size(); i++){
                string reducer_file = directory_name + "/" + files[i].fname().substr(files[i].fname().find_last_of("/")+1);
                as.save_blob(files[i].fname(), reducer_file);
                reducer_file_names.push_back(reducer_file);
            }
            cout << "Task id " << task->task_id() <<  " " << reducer_file_names.size() << endl;
            string temp_out_file = directory_name + "/" + "temp";

            string azure_path = "/code/src/app/reducer.py";
            string vm_path = "/vagrant/workshop6-c/src/app/reducer.py";
            
            for(int i=0 ; i<files.size(); i++){
                if(i == 0){
                    execute(reducer_file_names[i], temp_out_file, azure_path, "reducer.py",  O_RDWR|O_CREAT); 
                } else { 
                    execute(reducer_file_names[i], temp_out_file, azure_path, "reducer.py",  O_RDWR|O_APPEND); 
                }
            }

            string final_out_file = directory_name + "/" + "final_" +  to_string(task->task_id()) + ".txt";
            execute(temp_out_file, final_out_file, azure_path, "reducer.py",  O_RDWR|O_CREAT); 
            
            as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
            as.upload_file(final_out_file, final_out_file);
            output_files.push_back(final_out_file);
            return output_files;
        }

        void executeTask(string worker_id_zookeeper) {
            if (my_tasks.size() == 0)
                return;
            update_worker_self_status(worker_id_zookeeper+ "/status", "BUSY");
			
            Task* task = my_tasks.front();

            string task_type = task -> task_type();
            LOG(INFO) << "Task type being executed : " << task_type <<endl;
            my_tasks.pop();
            string job_dir = "./job_" + to_string(task->job_id());
            vector<string> directories = {job_dir, job_dir + "/intermediates", job_dir + "/reducers"};
            
            for (auto directory: directories) {
                if (!boost::filesystem::exists(directory)) {
                    boost::filesystem::create_directory(directory);
                    cout << "Directory created" << endl;
                }
            }
            
            vector<string> output_files;

            
            if(task_type == "map")
                output_files = map(task);
            else
                output_files = reduce(task);
            
            cout << "Task completed. Sending to " <<  task->master_url() << endl;
            WorkerClient(task->master_url()).sendTaskCompletion(task, output_files);
            update_worker_self_status(worker_id_zookeeper+ "/status", "IDLE");
			
            
        }
};


class DynamicRPCListener {
	public:

	DynamicRPCListener(WorkerService::AsyncService* service, ServerCompletionQueue* cq, TaskExecutor* worker)
		: workerService(service), theCompletionQueue(cq), responderObject(&serverContext), status_(CREATE), worker(worker)
	{
		Proceed();
	}

	void Proceed() {
		if (status_ == CREATE) {
			status_ = PROCESS;
			workerService->Requestexecute_task(&serverContext, &task, &responderObject, theCompletionQueue, theCompletionQueue, this);
		
		} else if (status_ == PROCESS) {
			new DynamicRPCListener(workerService, theCompletionQueue, worker);
			cerr << "Received task from master" << endl;
			status_ = FINISH;
			reception.set_message("Received task");
            if(worker->my_tasks.size() == 0){
			    worker->my_tasks.push(&task);
			    responderObject.Finish(reception, Status::OK, this);
            } else {
                responderObject.Finish(reception, Status::CANCELLED, this);
            }
			
		} else {
			GPR_ASSERT(status_ == FINISH);
			delete this;
		}
	}

	private:
	TaskExecutor* worker;
	WorkerService::AsyncService* workerService;
	CompletionQueue asyncCallsQueue;
	ServerCompletionQueue* theCompletionQueue;
	ServerContext serverContext;
	Task task;
	TaskReception reception;
	ServerAsyncResponseWriter<TaskReception> responderObject;
	
	enum CallStatus { CREATE, PROCESS, FINISH };
	CallStatus status_; 
}; 


class MultiThreadedDispatchServer final {
	public:

    string status;
    string worker_id_zookeeper;

	MultiThreadedDispatchServer(TaskExecutor* worker_reference, string my_id) : worker(worker_reference) {
        status = "IDLE";
        my_id = worker_id_zookeeper;
    }

	~MultiThreadedDispatchServer() {
		server->Shutdown();
		theCompletionQueue->Shutdown();
	}
	
	void runServer(std::string workerServerAddress) {
		ServerBuilder builder;
		builder.AddListeningPort(workerServerAddress, grpc::InsecureServerCredentials());
		builder.RegisterService(&workerService);
		theCompletionQueue = builder.AddCompletionQueue();
		server = builder.BuildAndStart();
		std::cerr << "Worker Server listening on " << workerServerAddress << std::endl;
		HandleRpcs();
	}
	
	private:
	
	void HandleRpcs() {
		new DynamicRPCListener(&workerService, theCompletionQueue.get(), worker);
		void* tag; 
		bool ok;
		while (true) {
			GPR_ASSERT(theCompletionQueue->Next(&tag, &ok));
			GPR_ASSERT(ok);
			static_cast<DynamicRPCListener*>(tag)->Proceed();
            worker->executeTask(worker_id_zookeeper);			
		}
	}

	std::unique_ptr<ServerCompletionQueue> theCompletionQueue;
	WorkerService::AsyncService workerService;
	std::unique_ptr<Server> server;
	TaskExecutor* worker;
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

/* sample usage
    update_worker_self_status(/workers/workerid/status, IDLE)
    update_worker_self_status(/workers/workerid/busy, IDLE)
*/
string register_with_zoopeeker(string local_ip){
    cout << "Elect leader called by " << local_ip << endl;

    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    unique_ptr<ConservatorFramework> framework = factory.newClient("127.0.0.1:2181");
    framework->start();

    cout << "Connected to the zookeeper service" << endl;
    
    auto res = framework->create()->forPath("/workers");
    cout << "Create /workers retval:" << res << endl;
    
    res = framework->checkExists()->forPath("/workers");
    assert(res == ZOK);
    cout << "/workers now exists"<< endl;

    string realpath;
    int mypath;
    
    res = framework->create()->withFlags(ZOO_EPHEMERAL | ZOO_SEQUENCE)->forPath("/workers/" + local_ip + "_", NULL, realpath);
    if (res != ZOK) {
        cout << "Failed to create ephemeral node, retval "<< res << endl;;
    } else {
        cout << "Created seq ephm node " << realpath << endl;
        mypath = stoi(realpath.substr(realpath.find('_') + 1));
    }
    cout << "MY ID in zookeeper " << realpath << endl;; 
    return realpath;
}

void runServer(string zookeeper_id) {
    std::string server_address("0.0.0.0:5001");
    TaskExecutor executor;
    MultiThreadedDispatchServer(&executor, zookeeper_id).runServer(server_address);
}

int main(int argc, char** argv) {
    string zookeeper_id = register_with_zoopeeker(get_local_ip());
    runServer(zookeeper_id);
    return 0;
}
