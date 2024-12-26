// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>
#include <string>

#include "absl/strings/str_cat.h"
#include "google/cloud/storage/client.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/blob_storage_provider/gcp/gcp_cloud_storage.h"
#include "src/core/interface/blob_storage_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::test {
namespace {

using google::cloud::Options;
using google::cloud::storage::BucketMetadata;
using google::cloud::storage::Client;
using google::cloud::storage::ProjectIdOption;
using google::scp::core::AsyncExecutor;
using google::scp::core::GetBlobRequest;
using google::scp::core::GetBlobResponse;
using google::scp::core::blob_storage_provider::GcpCloudStorageClient;
using google::scp::core::errors::GetErrorMessage;

constexpr std::string_view kProject = "admcloud-coordinator1";

constexpr std::string_view kBucketName = "test-bucket";

constexpr std::string_view kDefaultBlobName = "blob";
constexpr char kBlobByte = 'a';

constexpr size_t kThreadCount = 5;
constexpr size_t kQueueSize = 100;

int ClearBucketIfPresent(Client& client) {
  auto buckets_list = client.ListBuckets();
  if (std::any_of(
          buckets_list.begin(), buckets_list.end(),
          [](const auto& bucket) { return bucket->name() == kBucketName; })) {
    for (auto&& obj_metadata : client.ListObjects(kBucketName)) {
      if (!obj_metadata) {
        std::cout << "Failed listing: " << obj_metadata.status().message()
                  << std::endl;
        return EXIT_FAILURE;
      }
      if (auto status = client.DeleteObject(kBucketName, obj_metadata->name());
          !status.ok()) {
        std::cout << status.message() << std::endl;
        return EXIT_FAILURE;
      }
    }
  }
  return EXIT_SUCCESS;
}

void DeleteBucket(Client& client) {
  ClearBucketIfPresent(client);
  client.DeleteBucket(kBucketName);
}

void CreateBucketIfNotExists(Client& client) {
  auto buckets_list = client.ListBuckets();
  if (std::none_of(
          buckets_list.begin(), buckets_list.end(),
          [](const auto& bucket) { return bucket->name() == kBucketName; })) {
    client.CreateBucket(kBucketName, BucketMetadata());
  }
}

void WriteObjectOfByteCount(GcpCloudStorageClient& client, int64_t byte_count) {
  std::atomic_bool finished(false);
  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
  put_blob_context.request = std::make_shared<PutBlobRequest>(
      PutBlobRequest{{std::make_shared<std::string>(kBucketName),
                      std::make_shared<std::string>(kDefaultBlobName)}});

  ExecutionResult result;
  put_blob_context.callback = [&result, &finished](auto& context) {
    result = context.result;
    finished = true;
  };
  put_blob_context.request->buffer = std::make_shared<BytesBuffer>(byte_count);
  put_blob_context.request->buffer->length =
      put_blob_context.request->buffer->capacity;
  put_blob_context.request->buffer->bytes->assign(byte_count, kBlobByte);

  ::assert(client.PutBlob(put_blob_context).Successful());

  while (!finished) {
  }

  if (!result.Successful()) {
    std::cerr << errors::GetErrorMessage(result.status_code) << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

int WriteAndGetBlob(int64_t byte_count) {
  auto client =
      std::make_shared<Client>(Options{}.set<ProjectIdOption>(kProject));

  if (ClearBucketIfPresent(*client) != EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }

  CreateBucketIfNotExists(*client);

  auto async_executor =
      std::make_shared<AsyncExecutor>(kThreadCount, kQueueSize);
  auto io_async_executor =
      std::make_shared<AsyncExecutor>(kThreadCount, kQueueSize);

  async_executor->Init();
  async_executor->Run();
  io_async_executor->Init();
  io_async_executor->Run();

  GcpCloudStorageClient my_client(client, async_executor, io_async_executor,
                                  AsyncPriority::Normal, AsyncPriority::Normal);

  WriteObjectOfByteCount(my_client, byte_count);

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.request = std::make_shared<GetBlobRequest>(GetBlobRequest{
      std::make_shared<std::string>(kBucketName),
      std::make_shared<std::string>(kDefaultBlobName),
  });

  int return_status;
  std::atomic_bool finished(false);
  get_blob_context.callback = [&return_status, &finished, client,
                               byte_count](auto& context) {
    if (!context.result.Successful()) {
      std::cout << "GetBlob execution failed with: "
                << GetErrorMessage(context.result.status_code) << std::endl;
      return_status = EXIT_FAILURE;
      return;
    }
    if (auto response = context.response; response) {
      if (response->buffer->bytes->size() != byte_count) {
        std::cout << "bad byte count: " << response->buffer->bytes->size()
                  << " vs. " << byte_count << std::endl;
        return_status = EXIT_FAILURE;
        return;
      }
      int i = 0;
      for (Byte b : *response->buffer->bytes) {
        if (b != kBlobByte) {
          std::cout << b << " does not equal '" << kBlobByte
                    << "' at index: " << i;
          return_status = EXIT_FAILURE;
          return;
        }
        i++;
      }
      return_status = EXIT_SUCCESS;
    }
    finished = true;
  };

  if (auto sync_result = my_client.GetBlob(get_blob_context);
      !sync_result.Successful()) {
    std::cout << "GetBlob failed with: "
              << GetErrorMessage(sync_result.status_code) << std::endl;
    return_status = EXIT_FAILURE;
    finished = true;
  }

  while (!finished) {
  }

  async_executor->Stop();
  io_async_executor->Stop();

  if (return_status != EXIT_SUCCESS) {
    DeleteBucket(*client);
  }

  return return_status;
}

}  // namespace
}  // namespace google::scp::core::test

constexpr int64_t kBytesCount = 100;
constexpr int64_t kKiloBytesCount = 1000;
constexpr int64_t kMegaBytesCount = 1000 * kKiloBytesCount;
constexpr int64_t kGigaBytesCount = 1000 * kMegaBytesCount;
constexpr int64_t k10GigaBytesCount = 10 * kGigaBytesCount;
constexpr int64_t kTeraBytesCount = 1000 * kGigaBytesCount;

int main(int argc, char* argv[]) {
  for (auto count :
       {kBytesCount, kKiloBytesCount, kMegaBytesCount, kGigaBytesCount,
        /*, k10GigaBytesCount, kTeraBytesCount*/}) {
    if (google::scp::core::test::WriteAndGetBlob(count) != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }
  std::cout << "Succeeded" << std::endl;
}
