#include "blob.h"

#include <cpprest/containerstream.h>
#include <cpprest/filestream.h>
#include <was/blob.h>
#include <was/storage_account.h>

#include <cassert>
#include <string>

#include "trace.h"

#include "stdafx.h"

using namespace std;

AzureStorageHelper::AzureStorageHelper(std::string connection_str,
                           std::string container_name)
    : connection_str_(connection_str), container_name_(container_name) {
  this->storage_account_ =
      azure::storage::cloud_storage_account::parse(this->connection_str_);
  this->client_ = this->storage_account_.create_cloud_blob_client();
  this->container_ =
      this->client_.get_container_reference(this->container_name_);
  this->container_.create_if_not_exists();

  azure::storage::blob_container_permissions permissions;
  permissions.set_public_access(
      azure::storage::blob_container_public_access_type::blob);
  this->container_.upload_permissions(permissions);
}

void AzureStorageHelper::upload_file(std::string filename, std::string blobname) {
  Trace trace(__func__, "file=" + filename + ", blob=" + blobname);
  concurrency::streams::istream input_stream =
      concurrency::streams::file_stream<uint8_t>::open_istream(filename).get();
  auto file_blob = this->container_.get_block_blob_reference(blobname);
  file_blob.upload_from_stream(input_stream);
  input_stream.close().wait();
}

void AzureStorageHelper::upload_stream(concurrency::streams::istream &stream,
                                 std::size_t size, std::string blobname) {
  Trace trace(__func__, "blob=" + blobname);
  auto file_blob = this->container_.get_block_blob_reference(blobname);
  file_blob.upload_from_stream(stream, size);
}

std::string AzureStorageHelper::get_blob(std::string blobname) {
  Trace trace(__func__, "blob=" + blobname);
  concurrency::streams::container_buffer<std::vector<uint8_t>> buffer;
  concurrency::streams::ostream output_stream(buffer);

  auto blob = this->container_.get_block_blob_reference(blobname);
  blob.download_to_stream(output_stream);  
  std::vector<unsigned char> &data = buffer.collection();
  return std::string((char *)&data[0], data.size());
}


std::string AzureStorageHelper::get_blob_with_offset(std::string blobname, int start, int length) {
  Trace trace(__func__, "blob=" + blobname);
  concurrency::streams::container_buffer<std::vector<uint8_t>> buffer;
  concurrency::streams::ostream output_stream(buffer);
  
  auto blob = this->container_.get_block_blob_reference(blobname);
  blob.download_range_to_stream(output_stream, start, length);
  std::vector<unsigned char> &data = buffer.collection();
  return std::string((char *)&data[0], data.size());
}

void AzureStorageHelper::save_blob(std::string blobname, std::string filename) {
  Trace trace(__func__, "file=" + filename + ", blob=" + blobname);
  auto blob = this->container_.get_block_blob_reference(blobname);
  blob.download_to_file(filename);
}

int AzureStorageHelper::get_blob_size(std::string blobname){
  Trace trace(__func__, " blob=" + blobname);
  auto blob = this->container_.get_block_blob_reference(blobname);
  blob.download_attributes();
  blob.download_account_properties();
  return blob.properties().size();
}

void AzureStorageHelper::delete_all(void) {
  Trace trace(__func__);
  for (auto it = this->container_.list_blobs();
       it != azure::storage::list_blob_item_iterator{}; ++it) {
    if (it->is_blob()) {
      it->as_blob().delete_blob();
    }
  }
}
