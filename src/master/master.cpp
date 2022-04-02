#include <zookeeper/zookeeper.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <chrono>
#include <queue>
#include <vector>
#include <thread>

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "central.grpc.pb.h"
#include "sharding.h"
#include "task.h"
#include "worker.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using mapr::WorkerService;
using mapr::ResultFile;
using mapr::Task;
using mapr::TaskCompletion;
using mapr::TaskReception;
using mapr::FileInfo;

using namespace std;


class MasterClient { 
 
    public:
        MasterClient(std::shared_ptr<Channel> channel): stub_(WorkerService::NewStub(channel)) {}

        vector<string> execute_task(shared_ptr<TaskInstance> task_instance, shared_ptr<WorkerInstance> worker) {
            vector<string> output_files;
            Task task;
            string task_type;
            
            if (task_instance->taskType == TaskType::map)
                task_type = "map";
            else
                task_type = "reduce";

            task.set_task_type(task_type);
            task.set_task_id(task_instance->id);
            task.set_num_reducers(3);
            
            FileInfo* file_info = task.add_files();
            
            for (auto file: task_instance->shard->files) {
                file_info->set_fname(file.fileName);
                file_info->set_start(file.startOffset);
                file_info->set_end(file.endOffset);
            }

            TaskCompletion completion;
            ClientContext context;

            Status status = stub_->execute_task(&context, task, &completion);

            if (status.ok()) {
                cout << "Map task completed" << " " << task_instance->id << endl;
                std::vector<ResultFile> files(completion.result_files().begin(), completion.result_files().end());
                for (auto file: files)
                    output_files.push_back(file.filename());

            } else {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            }
            return output_files;
        }
    private:
        std::unique_ptr<WorkerService::Stub> stub_;
};


class Master {

    private:
        queue<shared_ptr<TaskInstance>> tasks_;
        vector<shared_ptr<WorkerInstance>> workers_;
        vector<string> map_phase_files;
        int phase = 0;
    
    public:
        Master() {}

        vector<shared_ptr<ShardAllocation>> shard() {
            vector<shared_ptr<ShardAllocation>> allShards = createShardAllocations();
            LOG(INFO) << "Sharding phase complete" << endl;
            LOG(INFO) << "Num Shards: " << allShards.size() << endl;
            return allShards;
        }

        int choose_worker() {
            for (int i = 0; i < workers_.size(); i++) {
                auto worker = workers_[i];
                if (worker->status == WorkerStatus::idle)
                    return i;
            }
            return -1;
        }

        void schedule(string phase) {
            cout << phase << " phase started" << endl;
            while (!tasks_.empty()) {
                auto task = tasks_.front();
                tasks_.pop();
                int worker_idx = choose_worker();
                if (worker_idx == -1)
                    continue;    
                auto worker = workers_[worker_idx];
                cout << "For task id: " << task->id << " worker is: " << worker->id << endl;
                trigger(task, worker);
            }
        }
        
        void trigger(shared_ptr<TaskInstance> task, shared_ptr<WorkerInstance> worker) {
            string serverAddress = worker->address;
            LOG(INFO) << "The server address is " << serverAddress << endl;
            MasterClient masterClient(worker->channel);
            vector<string> op_files = masterClient.execute_task(task, worker);
            if (task->taskType == TaskType::map) {
                for (auto file: op_files)
                    map_phase_files.push_back(file);
            }
        }

        void populateWorkers() {
            auto workers = WorkerInstance::populate();
            workers_ = workers;
        }

        void execute() {
            populateWorkers();
            auto shards = shard();
            int id = 1;
            for (auto shard: shards) {
                auto task = make_shared<TaskInstance>(id, TaskType::map, shard);
                tasks_.push(task);
                id = id + 1;
            }
            schedule("map");
            cout << "Map phase complete." << endl;
            phase = phase + 1;
            cout << "There are " << to_string(map_phase_files.size()) << " files" << endl;
            std::map<int, vector<string>> reducer_wise_files;
            
            for (auto filename: map_phase_files) {
                int start = filename.find_last_of('_');
                int end = filename.find('.');
                int reducer_id = stoi(filename.substr(start + 1));
                reducer_wise_files[reducer_id].push_back(filename);
            }

            std::map<int, vector<string>>::iterator it;
            for (it = reducer_wise_files.begin(); it != reducer_wise_files.end(); it++) {
                shared_ptr<ShardAllocation> newShard = shared_ptr<ShardAllocation>(new ShardAllocation());
                for (auto filename: it->second) {
                    ShardFileInfo fileInfo;
                    fileInfo.fileName = filename;
                    fileInfo.startOffset = 0;
                    fileInfo.endOffset = 10;
                    newShard->files.push_back(fileInfo);
                }
                auto task = make_shared<TaskInstance>(id, TaskType::reduce, newShard);
                task->reducer_id = it->first;
                tasks_.push(task);
                id = id + 1;
            }

            schedule("reduce");
        }

};


int get_node_id(const std::string &s) {
  return stoi(s.substr(s.find('_') + 1));
}


bool electLeader(string hostname) {
    LOG(INFO) << "Elect leader called by " << hostname << endl;

    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    unique_ptr<ConservatorFramework> framework = factory.newClient("default-zookeeper:2181");
    framework->start();

    LOG(INFO) << "Connected to the zookeeper service" << endl;
    
    auto res = framework->create()->forPath("/masters");
    LOG(INFO) << "Create /masters retval:" << res;
    
    res = framework->checkExists()->forPath("/masters");
    assert(res == ZOK);
    LOG(INFO) << "/masters now exists";

    string realpath;
    int mypath;
    
    res = framework->create()->withFlags(ZOO_EPHEMERAL | ZOO_SEQUENCE)->forPath("/masters/" + hostname + "_", NULL, realpath);
    if (res != ZOK) {
      LOG(FATAL) << "Failed to create ephemeral node, retval "<< res;
    } else {
      LOG(INFO) << "Created seq ephm node " << realpath;
      mypath = stoi(realpath.substr(realpath.find('_') + 1));
    }

    bool is_leader = false;
    while (!is_leader) {
        std::vector<std::string> everyone = framework->getChildren()->forPath("/masters");
        
        int minimal_node_id = 1000;
        LOG(INFO) << "My node id " << mypath << endl;
        
        for (auto element: everyone) {
            int node_id = get_node_id(element);
            LOG(INFO) << "The node id is " << node_id << endl;
            
            if (node_id < minimal_node_id)
                minimal_node_id = node_id;
        }
        
        LOG(INFO) << "Chosen node id " << minimal_node_id << endl;
        
        if (minimal_node_id == mypath) {
            LOG(INFO) << hostname << " is the leader" << endl;
            is_leader = true;
        } else {
            LOG(INFO) << hostname << " is waiting to become leader";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    return true;
}


int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_stderrthreshold = 0;
    LOG(INFO) << "The Master has started" << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    string hostname;
    
    array<char, 1024> buffer;
    FILE* out = popen("hostname -i", "r");
    if (!out) {
        LOG(FATAL) << "Popen failed";
    }
    int n_read;
    while (n_read = fgets(buffer.data(), 512, out) != NULL) {
        hostname += buffer.data();
    }
    pclose(out);
    if (!hostname.empty() && hostname.back() == '\n') {
        hostname.pop_back();
    }
    
    LOG(INFO) << "The host name is " << hostname << endl;
    // electLeader(hostname);
    Master master;
    master.execute();
    
    while (true) {
        LOG(INFO) << "Waiting to be killed (sleeping for 2 seconds)";
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}