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
#include <mutex>
#include <crow.h>

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "central.grpc.pb.h"
#include "master.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerBuilder;

using mapr::MasterService;
using mapr::WorkerService;
using mapr::ResultFile;
using mapr::Task;

using mapr::TaskCompletion;
using mapr::TaskCompletionAck;
using mapr::TaskReception;
using mapr::FileInfo;

using namespace std;

class Master;

class MasterServiceImpl final : public MasterService::Service {

    private:
        Master* master;
    public:

        MasterServiceImpl(Master* master) {
            this->master = master;
        }
        
        Status task_complete(
            ServerContext* context, 
            const TaskCompletion* completion,
            TaskCompletionAck* ack
        ) override {
            LOG(INFO) << "Task complete received for " << completion->task_id() << endl;

            std::vector<ResultFile> files(completion->result_files().begin(), completion->result_files().end());
            
            
            std::unique_lock<std::mutex> lck (master->task_mutex, std::defer_lock);
            
            lck.lock();
            
            shared_ptr<Task> task = master->get_task_by_id(completion->task_id());
            task->set_status("completed");
            auto worker = master->get_worker_by_id(completion->worker_id());
            worker->status = WorkerStatus::idle;
            
            for (auto file: files) {
                ResultFile* result_file = task->add_output_files();
                result_file->set_filename(file.filename());
            }
            lck.unlock();
            
            ack->set_message("Acknowledged");
            return Status::OK;
        }

};


class MasterClient { 
 
    public:
        MasterClient(std::shared_ptr<Channel> channel): stub_(WorkerService::NewStub(channel)) {}

        bool execute_task(shared_ptr<Task> task, shared_ptr<WorkerInstance> worker) {
            vector<string> output_files;
            string task_type;
            
            TaskReception reception;
            ClientContext context;

            Status status = stub_->execute_task(&context, *task, &reception);

            if (status.ok()) {
                LOG(INFO) << "Receipt for " << task->task_id() << endl;
                return true;
            }
            else {
                LOG(INFO) << status.error_code() << ": " << status.error_message() << std::endl;
                return false;
            }
        }
    private:
        std::unique_ptr<WorkerService::Stub> stub_;
};

Master::Master() {
    kill_server = false;
}

shared_ptr<Task> Master::get_task_by_id(int task_id) {
    for (auto task: tasks_) {
        if (task->task_id() == task_id)
            return task;
    }
    return NULL;
}


shared_ptr<WorkerInstance> Master::get_worker_by_id(int worker_id) {
    for (auto worker: workers_) {
        if (worker->id == worker_id)
            return worker;
    }
    return NULL;
}


vector<shared_ptr<ShardAllocation>> Master::shard() {
    vector<shared_ptr<ShardAllocation>> allShards = createShardAllocations(job->shard_size, job->file_paths);
    LOG(INFO) << "Sharding phase complete" << endl;
    LOG(INFO) << "Num Shards: " << allShards.size() << endl;
    return allShards;
}

int Master::choose_worker() {
    LOG(INFO) << "Number of workers available: " << workers_.size() << endl;
    for (int i = 0; i < workers_.size(); i++) {
        if (workers_[i]->status == WorkerStatus::idle)
            return i;
    }
    return -1;
}

void Master::schedule(string phase) {
    LOG(INFO) << phase << " phase started" << endl;
    cout << "Total tasks: " << tasks_.size() << endl;
    
    while (true) {
        int tasks_count = tasks_.size();
        int tasks_comp = 0;

        std::unique_lock<std::mutex> lck (task_mutex, std::defer_lock);
        lck.lock();

        for (auto task: tasks_) {
            if (task->status() == "completed")
                tasks_comp += 1;
            
            if (task->status() != "queued")
                continue;
            
            int worker_idx = choose_worker();
            if (worker_idx == -1) {
                continue;
            }
            if (!trigger(task, workers_[worker_idx])) {
                LOG(INFO) << "Trigger unsuccessful for task: " << task->task_id() << " and worker: " << workers_[worker_idx]->id << endl;
                continue;
            }
            task->set_status("running");
            workers_[worker_idx]->status = WorkerStatus::busy;
        }
        lck.unlock();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        if (tasks_comp == tasks_count)
            break;
    }

    
}

void Master::runServer() {
    std::string server_address("0.0.0.0:5002");
    MasterServiceImpl service(this);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Master Server listening on " << server_address << std::endl;
    
    while (!kill_server)
        server->Wait();
    cout << "Server shutdown" << endl;
    server->Shutdown();
}

bool Master::trigger(shared_ptr<Task> task, shared_ptr<WorkerInstance> worker) {
    string serverAddress = worker->address;
    MasterClient masterClient(worker->channel);
    string master_url = master_host_name + ":5002";
    task->set_master_url(master_url);
    task->set_worker_id(worker->id);
    return masterClient.execute_task(task, worker);
}

void Master::populateWorkers() {
    workers_.clear();
    while (true) {
        auto workers = WorkerInstance::populate();
        if (workers.size() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        workers_ = workers;
        return;
    }
}

void Master::bootstrap_tasks() {
    auto shards = shard();
    int id = 1;
    
    for (auto shard: shards) {
        auto task = make_shared<Task>();

        task->set_task_id(id);
        task->set_job_id(job->job_id);
        task->set_status("queued");

        // Change this using the received configuration.
        task->set_num_reducers(3);
        
        task->set_task_type("map");
        
        for (auto file: shard->files) {
            FileInfo* file_info = task->add_files();
            file_info->set_fname(file.fileName);
            file_info->set_start(file.startOffset);
            file_info->set_end(file.endOffset);
        }
        
        tasks_.push_back(task);
        id = id + 1;
    }
}

void Master::fill_tasks(bool is_new) {
    if (is_new)
        bootstrap_tasks();
    
}

void Master::bootstrap_reduce() {
    std::map<int, vector<string>> reducer_wise_files;
    for (auto comp_task: tasks_) {
        for (auto result_file: comp_task->output_files()) {
            auto filename = result_file.filename();
            int start = filename.find_last_of('_');
            int end = filename.find('.');
            int reducer_id = stoi(filename.substr(start + 1));
            reducer_wise_files[reducer_id].push_back(filename);
        }
    }
    tasks_.clear();
    
    int id = 1;
    std::map<int, vector<string>>::iterator it;
    for (it = reducer_wise_files.begin(); it != reducer_wise_files.end(); it++) {
        
        auto task = make_shared<Task>();
        task->set_reducer_id(it->first);
        task->set_task_id(id);
        task->set_job_id(job->job_id);
        task->set_status("queued");
        task->set_task_type("reduce");
        
        for (auto filename: it->second) {
            FileInfo* file_info = task->add_files();
            file_info->set_fname(filename);
            file_info->set_start(0);
            file_info->set_end(100);
        }
        tasks_.push_back(task);
        id = id + 1;
    }
}

bool Master::get_next_job() {
    vector<string> jobs;
    auto zoo_keeper = ZookeeperHelper();
    zoo_keeper.get_jobs_ordered(jobs, false);
    
    for (auto job: jobs) {
        cout << "Job" << job << endl;
        string zoo_id = job.substr(job.find('_') + 1);
        int job_id = stoi(job.substr(job.find('_') + 1));
        
        string job_dir = "/jobs/" + job;
        
        auto status = zoo_keeper.get_data(job_dir + "/status");
        if(status != "CREATED"){
            continue;
        }
        cout << "Status" << status;
        auto shard_size = zoo_keeper.get_data(job_dir + "/shard_size");
        auto reducer_count = zoo_keeper.get_data(job_dir + "/reducer_count");
        auto files = zoo_keeper.get_data(job_dir + "/files");

        vector<string> file_names;
        boost::split(file_names, files, boost::is_any_of("$"), boost::token_compress_on);

        cout << "Shard" << shard_size << endl;
        cout << "Reducer" << reducer_count << endl;
        cout << "Files count" << file_names.size() << endl;
        
        for (auto file: file_names) 
            cout << "File" << file << endl;

        this->job = make_shared<Job>(job_id, file_names, stoi(shard_size), stoi(reducer_count), zoo_id);
        return true;
    }
    cout << "No incomplete jobs available" << endl;
    return false;
}

void Master::execute() {

    LOG(INFO) << "Spawning thread for listening to task completions" << endl;
    std::thread server_thread(&Master::runServer, this);

    while (true) {
        bool status = get_next_job();
        if (!status) {
            cout << "Next job poll in 5 secs" << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            continue;
        }

        cout << "Working on Job id: " << job->job_id << endl;
        string hostname = get_local_ip();
        master_host_name = hostname;
    
        populateWorkers();
        fill_tasks(true);
        
        schedule("map");
        LOG(INFO) << "Map phase complete." << endl;
        bootstrap_reduce();
        schedule("reduce");
        LOG(INFO) << "Reduce phase complete." << endl;
        job_end();
        cout << "Next job poll in 5 secs" << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100000));
    cout << "Killing server" << endl;
    kill_server = true;
    server_thread.join();
}

void Master::job_end() {
    tasks_.clear();
    string path = "/jobs/job_" + job->zoo_id + "/status";
    auto zoo_keeper = ZookeeperHelper();
    zoo_keeper.set(path, "COMPLETED");
    job->is_complete = true;

}
        

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
    electLeader(hostname);
    LOG(INFO) << "I am the leader " << hostname << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    Master master;
    master.execute();
    
    while (true) {
        LOG(INFO) << "Waiting to be killed (sleeping for 2 seconds)";
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}