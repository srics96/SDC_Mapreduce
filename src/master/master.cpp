#include <zookeeper/zookeeper.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <chrono>
#include <queue>
#include <thread>

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "central.grpc.pb.h"
#include "sharding.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using mapr::WorkerService;
using mapr::HandShakeRequest;
using mapr::HandShakeReply;
using mapr::Task;
using mapr::ShardData;

using namespace std;


class MasterClient { 
 
    public:
        MasterClient(std::shared_ptr<Channel> channel): stub_(WorkerService::NewStub(channel)) {}

        std::string handshake(string message) {
            HandShakeRequest request;
            request.set_message(message);

            HandShakeReply reply;

            ClientContext context;

            Status status = stub_->handshake(&context, request, &reply);

            if (status.ok()) {
                return reply.message();
            } else {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            }
        
        }

        std::string map(string tasktype, string taskid, string fname, int start, int end) {
            Task task;
            task.set_tasktype(tasktype);
            task.set_taskid(taskid);
            
            ShardData *shard = new ShardData(); 
            shard->set_fname(fname);
            shard->set_end(end);
            shard->set_start(start);
            
            task.set_allocated_mapshard(shard);
            

            HandShakeReply reply;

            ClientContext context;

            Status status = stub_->map(&context, task, &reply);

            if (status.ok()) {
                return reply.message();
            } else {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            }
        
        }
    private:
        std::unique_ptr<WorkerService::Stub> stub_;
};


class Master {

    private:


    public:
        Master() {}

        void shard() {
            vector<shared_ptr<ShardAllocation>> allShards = createShardAllocations();
            LOG(INFO) << "Sharding phase complete" << endl;
        }

        void trigger() {
            string serverAddress = "localhost:5001";
            LOG(INFO) << "The server address is " << serverAddress << endl;
            MasterClient masterClient(grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials()));
            LOG(INFO) << "Connected to server" << endl;

            string tasktype("Map"), taskid("99999");
            int start=3, end=347; 
            string reply = masterClient.map(tasktype, taskid, "gutenberg/Winston Churchill___Coniston, Complete.txt", start, end); 
            LOG(INFO) << "Handshake response received: " << reply << std::endl;
        }

        void execute() {
            trigger();
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