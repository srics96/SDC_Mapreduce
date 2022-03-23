#include <iostream>
#include <string>
#include <vector>

#include "../util/shard.h"

using namespace std;

enum TaskType {map, reduce}
enum StatusType {queued, running, failed}

class Task {
    private:
        TaskType taskType;
        string worker;
        StatusType status;
        shared_ptr<ShardAllocation> shard;

    Task(TaskType taskType, shared_ptr<ShardAllocation> shard) {
        this.taskType = taskType;
        this.status = queued;
        this.shard = shard;
    }
}