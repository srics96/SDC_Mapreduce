#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <map>

#include "../util/shard.h"
#include "../util/blob.h"
#include "../util/constants.h"

using namespace std;


void printShard(shared_ptr<ShardAllocation> shard) {
    cout << "-----------------------" << endl;
    cout << "ID: " << shard->id << endl;
    cout << "capacity: " << shard->capacity << endl;
    for (auto info: shard->files) {
        cout << "File: " << info.fileName << endl;
        cout << "start: " << info.startOffset << endl;
        cout << "end: " << info.endOffset << endl;
    }
    cout << "-----------------------" << endl;
}


vector<shared_ptr<ShardAllocation>> createShardAllocations(int shard_size, vector<string> filePaths) {
    // TODO - Remove hardcoded values.
    //vector<string> filePaths {"gutenberg/John Bunyan___The Works of John Bunyan.txt", "gutenberg/William Wordsworth___The Prose Works of William Wordsworth.txt"};
    
    int shardSize = shard_size;
    vector<shared_ptr<ShardAllocation>> allShards;
    shared_ptr<ShardAllocation> currentShard = shared_ptr<ShardAllocation>(new ShardAllocation());
    currentShard->id = 1;
    currentShard->capacity = 0;
    
    int n = filePaths.size();

    auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
    
    for (int i = 0; i < n; i++) {

        string filePath = filePaths[i];
        int fileSize = as.get_blob_size(filePath);
        int startOffset = 1;
        
        while (startOffset <= fileSize) {
            int requiredShardSize = shardSize - currentShard->capacity;

            startOffset = startOffset;
            int endOffset = min(fileSize, (startOffset + requiredShardSize) - 1);
            // cout << "For Shard " << currentShard->id << ", file: " << filePath << "size: " << fileSize;
            // cout << " start: " << startOffset << "end: " << endOffset << endl << endl;

            ShardFileInfo fileInfo;
            fileInfo.fileName = filePath;
            fileInfo.startOffset = startOffset;
            fileInfo.endOffset = endOffset;
            currentShard->files.push_back(fileInfo);
            int capacity = (endOffset - startOffset) + 1;
            currentShard->capacity = currentShard->capacity + capacity;

            bool isShardComplete = currentShard->capacity == shardSize;
            
            if (isShardComplete) {
                // cout << "Shard " << currentShard->id << " is complete." << endl;
                allShards.push_back(currentShard);
                shared_ptr<ShardAllocation> newShard = shared_ptr<ShardAllocation>(new ShardAllocation());
                newShard->id = currentShard->id + 1;
                newShard->capacity = 0;
                currentShard = newShard;
            }

            bool isLastByte = endOffset == fileSize;

            if (isLastByte && i == n - 1 && !isShardComplete)
                allShards.push_back(currentShard);

            startOffset = endOffset + 1;
        }
    }
    return allShards;
}
