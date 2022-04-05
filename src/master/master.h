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
#include "worker.h"
#include "job.h"
#include "../util/service_discovery.h"

using namespace std;

using mapr::Task;

class Master {

    private:
        vector<shared_ptr<Task>> tasks_;
        vector<shared_ptr<WorkerInstance>> workers_;
        vector<string> map_phase_files;
        string master_host_name;
        shared_ptr<Job> job;
        friend class MasterServiceImpl;
        
        bool kill_server;
        
    public:
        Master();

        shared_ptr<Task> get_task_by_id(int task_id);
        vector<shared_ptr<ShardAllocation>> shard();
        int choose_worker();
        void schedule(string phase);
        void runServer();
        bool trigger(shared_ptr<Task> task, shared_ptr<WorkerInstance> worker);
        void populateWorkers();
        void bootstrap_tasks();
        void fill_tasks(bool is_new);
        void execute();
        void job_end();
};