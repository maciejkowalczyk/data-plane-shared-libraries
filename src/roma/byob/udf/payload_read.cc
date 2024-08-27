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

#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/udf/sample.pb.h"

using ::privacy_sandbox::server_common::byob::ReadPayloadRequest;
using ::privacy_sandbox::server_common::byob::ReadPayloadResponse;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!";
    return -1;
  }
  int fd = std::stoi(argv[1]);
  ReadPayloadRequest req;
  {
    ::google::protobuf::Any any;
    ::google::protobuf::io::FileInputStream input(fd);
    ::google::protobuf::util::ParseDelimitedFromZeroCopyStream(&any, &input,
                                                               nullptr);
    any.UnpackTo(&req);
  }
  ReadPayloadResponse response;
  int64_t payload_size = 0;
  for (const auto& p : req.payloads()) {
    payload_size += p.size();
  }
  response.set_payload_size(payload_size);
  ::google::protobuf::Any any;
  any.PackFrom(std::move(response));
  ::google::protobuf::util::SerializeDelimitedToFileDescriptor(any, fd);
  return 0;
}
