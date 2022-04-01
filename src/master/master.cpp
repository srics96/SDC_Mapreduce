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
using mapr::Task;
using mapr::TaskReception;
using mapr::FileInfo;

using namespace std;


class MasterClient { 
 
    public:
        MasterClient(std::shared_ptr<Channel> channel): stub_(WorkerService::NewStub(channel)) {}

        std::string execute_task(shared_ptr<TaskInstance> task_instance, shared_ptr<WorkerInstance> worker) {
            Task task;
            string task_type;
            
            if (task_instance->taskType == TaskType::map)
                task_type = "map";
            else
                task_type = "reduce";

            task.set_task_type(task_type);
            task.set_task_id(task_instance->id);
            
            FileInfo* file_info = task.add_files();
            
            for (auto file: task_instance->shard->files) {
                file_info->set_fname(file.fileName);
                file_info->set_start(file.startOffset);
                file_info->set_end(file.endOffset);
            }

            TaskReception reception;
            ClientContext context;

            Status status = stub_->execute_task(&context, task, &reception);

            if (status.ok()) {
                return reception.message();
            } else {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            }
        
        }
    private:
        std::unique_ptr<WorkerService::Stub> stub_;
};


class Master {

    private:
        queue<shared_ptr<TaskInstance>> tasks_;
        vector<shared_ptr<WorkerInstance>> workers_;
    public:
        Master() {}

        vector<shared_ptr<ShardAllocation>> shard() {
            vector<shared_ptr<ShardAllocation>> allShards = createShardAllocations();
            LOG(INFO) << "Sharding phase complete" << endl;
            LOG(INFO) << "Num Shards: " << allShards.size() << endl;
            return allShards;
        }

        shared_ptr<WorkerInstance> choose_worker() {
            for (auto worker: workers_) {
                if (worker->status == WorkerStatus::idle)
                    return worker;
            }
            return NULL;
        }

        void schedule() {
            cout << "Schedule phase" << endl;
            while (!tasks_.empty()) {
                auto task = tasks_.front();
                tasks_.pop();
                auto worker = choose_worker();
                if (worker == NULL)
                    continue;    
                cout << "For task id: " << task->id << " worker is: " << worker->id << endl;
                trigger(task, worker);
                cout << "Trigger complete" << endl;
            }
        }
        
        void trigger(shared_ptr<TaskInstance> task, shared_ptr<WorkerInstance> worker) {
            string serverAddress = worker->address;
            LOG(INFO) << "The server address is " << serverAddress << endl;
            MasterClient masterClient(worker->channel);
            LOG(INFO) << "Connected to server" << endl;
            string reply = masterClient.execute_task(task, worker);
            LOG(INFO) << "Handshake response received: " << reply << std::endl;
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
            schedule();
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