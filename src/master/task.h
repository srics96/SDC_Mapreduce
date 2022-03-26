#include <iostream>
#include <string>
#include <vector>

using namespace std;

enum TaskType {map, reduce};
enum StatusType {queued, running, failed};

class TaskInstance {
    private:
        TaskType taskType;
        string worker;
        StatusType status;
        shared_ptr<ShardAllocation> shard;

    public:
        TaskInstance(TaskType taskType, shared_ptr<ShardAllocation> shard) {
            this->taskType = taskType;
            this->status = queued;
            this->shard = shard;
        }
};