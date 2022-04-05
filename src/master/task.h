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
        shared_ptr<ShardAllocation> shard;
        
        TaskInstance(int id, TaskType taskType, shared_ptr<ShardAllocation> shard) {
            this->id = id;
            this->taskType = taskType;
            this->status = queued;
            this->shard = shard;
        }
};