#include <iostream>
#include <string>
#include <vector>

using namespace std;

enum TaskType {map, reduce};
enum StatusType {queued, running, failed};

class TaskInstance {
    
    public:
        int id;
        TaskType taskType;
        string worker;
        StatusType status;
        int reducer_id;
        int job_id;
        int num_reducers;
        shared_ptr<ShardAllocation> shard;
        
        TaskInstance(int id, TaskType taskType, shared_ptr<ShardAllocation> shard, int job_id) {
            this->id = id;
            this->taskType = taskType;
            this->status = queued;
            this->shard = shard;
            this->job_id = job_id;
            this->num_reducers = 3;
        }
};