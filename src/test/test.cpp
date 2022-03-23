#include <cpprest/containerstream.h>
#include <cpprest/filestream.h>
#include <was/blob.h>
#include <was/storage_account.h>

#include <string>
#include <cassert>

#include "../util/blob.h"
#include "../util/constants.h"

using namespace std;

int main(int argc, const char *argv[]) {
  assert(argc == 3);
  auto fname = argv[1];
  auto blobname = argv[2];

  auto as = AzureStorageHelper(AZURE_STORAGE_CONNECTION_STRING, AZURE_BLOB_CONTAINER);
  cout << as.get_blob_with_offset("test_blob", 100, 1000);
  return 0;
}
