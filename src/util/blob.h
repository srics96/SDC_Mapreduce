#ifndef BLOB_H
#define BLOB_H 1
#include <cpprest/containerstream.h>
#include <cpprest/filestream.h>
#include <was/blob.h>
#include <was/storage_account.h>

#include <string>

using namespace std;

class AzureStorageHelper {
 
  public:
  // Combination of the 
  std::string connection_str_;
  std::string container_name_;
  azure::storage::cloud_storage_account storage_account_;
  azure::storage::cloud_blob_container container_;
  azure::storage::cloud_blob_client client_;
  AzureStorageHelper(std::string connection, std::string container_name_);
  void upload_file(std::string filename, std::string blobname);
  void upload_stream(concurrency::streams::istream &stream, std::size_t size,
                     std::string blobname);
  std::string get_blob(std::string blobname);
  int get_blob_size(std::string blobname);
  std::string get_blob_with_offset(std::string blobname, int start, int length);
  void save_blob(std::string blobname, std::string filename);
  void delete_all(void);
};
#endif /* ifndef BLOB_H */
