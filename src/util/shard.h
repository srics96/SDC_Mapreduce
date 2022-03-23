#include <iostream>
#include <vector>

using namespace std;


struct ShardFileInfo {
    int startOffset;
    int endOffset;
    string fileName;
};

struct ShardAllocation {
    int id;
    vector<ShardFileInfo> files;
    int capacity;
};