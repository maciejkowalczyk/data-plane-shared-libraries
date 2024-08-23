// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/gvisor/udf/sample.pb.h"

using ::privacy_sandbox::server_common::gvisor::ReadPayloadRequest;
using ::privacy_sandbox::server_common::gvisor::ReadPayloadResponse;

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  if (argc < 2) {
    LOG(ERROR) << "Not enough arguments!";
    return -1;
  }
  int32_t fd;
  CHECK(absl::SimpleAtoi(argv[1], &fd))
      << "Conversion of file descriptor string to int failed";
  ReadPayloadRequest req;
  {
    google::protobuf::io::FileInputStream input(fd);
    google::protobuf::util::ParseDelimitedFromZeroCopyStream(&req, &input,
                                                             nullptr);
  }
  ReadPayloadResponse response;
  int64_t payload_size = 0;
  for (const auto& p : req.payloads()) {
    payload_size += p.size();
  }
  response.set_payload_size(payload_size);
  google::protobuf::Any any;
  any.PackFrom(std::move(response));
  google::protobuf::util::SerializeDelimitedToFileDescriptor(any, fd);
  return 0;
}
