// Copyright 2024 Google LLC
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

#include "src/public/cpio/cli/blob_storage_cli.h"

#include <memory>
#include <regex>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/container/node_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "src/public/core/interface/errors.h"
#include "src/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"

ABSL_FLAG(std::vector<std::string>, blob_paths, std::vector<std::string>({}),
          "List of blob paths in the format of "
          "`gs://<bucket_name>/<file_path_inside_bucket>` for GCP GCS or "
          "`s3://<bucket_name>/<file_path_inside_bucket>` for AWS S3.");
ABSL_FLAG(bool, exclude_directories, false,
          "Optional. Defaults to `false`. If true, exclude blobs that are "
          "directories in `cli blob list`.");

namespace google::scp::cpio::cli {

namespace {
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::BlobStorageClientFactory;
using google::scp::cpio::BlobStorageClientInterface;

constexpr std::string_view kBlobClientRpcGet = "get";
constexpr std::string_view kBlobClientRpcList = "list";
const absl::NoDestructor<absl::node_hash_set<std::string_view>>
    kSupportedBlobCommands({
        kBlobClientRpcGet,
        kBlobClientRpcList,
    });
const absl::NoDestructor<std::basic_regex<char>> blob_path_regex(std::regex{
    "(s3://|gs://)([a-z0-9_.-]+)/?(.+)?", std::regex::optimize});

}  // namespace

absl::StatusOr<std::vector<std::pair<std::string, std::string>>>
ParseBlobPaths() {
  std::vector<std::pair<std::string, std::string>> bucket_and_blob_names;
  for (auto& blob_path : absl::GetFlag(FLAGS_blob_paths)) {
    std::smatch match;
    if (!std::regex_match(blob_path, match, *blob_path_regex)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Blob path: [", blob_path, "] is not formatted correctly."));
    }
    std::string bucket_name(match[2].str());
    std::string blob_name;
    if (match.size() == 4) {
      blob_name = match[3].str();
    }
    bucket_and_blob_names.push_back(
        std::pair<std::string, std::string>(bucket_name, blob_name));
  }
  return bucket_and_blob_names;
}

absl::Status CheckInputCommand(std::string_view command) {
  if (!kSupportedBlobCommands->contains(command)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Blob client command: [", command, "] is not supported."));
  }
  return absl::OkStatus();
}

absl::Status CliBlobStorage::RunCli(std::string_view command) {
  if (absl::Status error = CheckInputCommand(command); !error.ok()) {
    return error;
  }

  auto buckets_and_blobs_result = ParseBlobPaths();
  if (!buckets_and_blobs_result.ok()) {
    return buckets_and_blobs_result.status();
  }
  std::vector<std::pair<std::string, std::string>> buckets_and_blobs =
      *buckets_and_blobs_result;

  bool exclude_directories = absl::GetFlag(FLAGS_exclude_directories);

  for (auto bucket_and_blob : buckets_and_blobs) {
    std::cout << "bucket_name: [" << bucket_and_blob.first << "] blob_name: ["
              << bucket_and_blob.second << "]" << std::endl;
    absl::Status result;
    if (command == kBlobClientRpcGet) {
      result = GetBlob(bucket_and_blob.first, bucket_and_blob.second);
    } else if (command == kBlobClientRpcList) {
      result = ListBlobs(bucket_and_blob.first, bucket_and_blob.second,
                         exclude_directories);
    }
    if (!result.ok()) {
      return result;
    }
  }
  return absl::OkStatus();
}

absl::Status CliBlobStorage::GetBlob(std::string_view bucket_name,
                                     std::string_view blob_name) {
  absl::Status result;
  absl::Notification finished;

  auto get_blob_request = std::make_shared<GetBlobRequest>();
  get_blob_request->mutable_blob_metadata()->set_bucket_name(bucket_name);
  get_blob_request->mutable_blob_metadata()->set_blob_name(blob_name);

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      std::move(get_blob_request), [&result, &finished](auto& context) {
        if (context.result.Successful()) {
          std::cout << "Got blob:\n"
                    << context.response->DebugString() << std::endl;
          result = absl::OkStatus();
        } else {
          result =
              absl::InternalError(GetErrorMessage(context.result.status_code));
        }
        finished.Notify();
      });

  if (absl::Status get_blob_error =
          blob_storage_client_->GetBlob(get_blob_context);
      !get_blob_error.ok()) {
    return absl::InternalError(
        absl::StrCat("Getting blob failed: ", get_blob_error));
  }
  finished.WaitForNotification();
  if (!result.ok()) {
    return absl::InternalError(
        absl::StrCat("Getting blob failed asynchronously: ", result));
  }
  return absl::OkStatus();
}

absl::Status CliBlobStorage::ListBlobs(std::string_view bucket_name,
                                       std::string_view blob_name,
                                       bool exclude_directories) {
  absl::Status result;
  absl::Notification finished;

  auto list_blobs_metadata_request =
      std::make_shared<ListBlobsMetadataRequest>();
  list_blobs_metadata_request->mutable_blob_metadata()->set_bucket_name(
      bucket_name);
  if (!blob_name.empty()) {
    list_blobs_metadata_request->mutable_blob_metadata()->set_bucket_name(
        blob_name);
  }
  list_blobs_metadata_request->set_exclude_directories(exclude_directories);

  AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
      list_blobs_metadata_context(
          std::move(list_blobs_metadata_request),
          [&result, &finished](auto& context) {
            if (context.result.Successful()) {
              std::cout << "Listed blobs:\n"
                        << context.response->DebugString() << std::endl;
              result = absl::OkStatus();
            } else {
              result = absl::InternalError(
                  GetErrorMessage(context.result.status_code));
            }
            finished.Notify();
          });

  if (absl::Status list_blobs_metadata_error =
          blob_storage_client_->ListBlobsMetadata(list_blobs_metadata_context);
      !list_blobs_metadata_error.ok()) {
    return absl::InternalError(
        absl::StrCat("Listing blobs failed: ", list_blobs_metadata_error));
  }
  finished.WaitForNotification();
  if (!result.ok()) {
    return absl::InternalError(
        absl::StrCat("Listing blobs failed asynchronously: ", result));
  }
  return absl::OkStatus();
}
}  // namespace google::scp::cpio::cli
