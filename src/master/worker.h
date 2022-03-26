#include <iostream>
#include <string>

using namespace std;

enum WorkerStatus {live, down};

class WorkerInstance {
    
    private:
        int id;
        WorkerStatus status;
        std::string address;
    
    public:
        WorkerInstance(int id, std::string address) {
            this->id = id;
            this->address = address;
            this->status = live;
        }

}