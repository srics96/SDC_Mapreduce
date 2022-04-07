#include <iostream>

#include <vector>

class Job {
    public:

        int job_id;
        string zoo_id;
        vector<string> file_paths;
        int shard_size;
        int num_reducers;
        bool is_complete;

        Job(int job_id, vector<string> file_paths, int shard_size, int num_reducers, string zoo_id) {
            this->job_id = job_id;
            this->file_paths = file_paths;
            this->shard_size = shard_size;
            this->num_reducers = num_reducers;
            this->is_complete = false;
            this->zoo_id = zoo_id;
        }
};
