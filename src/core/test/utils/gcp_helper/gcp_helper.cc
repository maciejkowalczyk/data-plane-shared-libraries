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

#include "gcp_helper.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include <google/pubsub/v1/pubsub.grpc.pb.h>

#include "absl/strings/str_cat.h"

using google::pubsub::v1::Publisher;
using google::pubsub::v1::Topic;
using grpc::ClientContext;
using grpc::Status;
using grpc::StubOptions;

namespace google::scp::core::test {
std::unique_ptr<Publisher::StubInterface> CreatePublisherStub(
    std::string_view endpoint) {
  auto channel = grpc::CreateChannel(std::string{endpoint},
                                     grpc::InsecureChannelCredentials());
  return Publisher::NewStub(channel, StubOptions());
}

void CreateTopic(Publisher::StubInterface& stub, std::string_view project_id,
                 std::string_view topic_id) {
  auto topic_name = absl::StrCat("projects/", project_id, "/topics/", topic_id);
  Topic topic;
  topic.set_name(topic_name);
  ClientContext client_context;
  Topic response;
  auto status = stub.CreateTopic(&client_context, topic, &response);
  if (!status.ok()) {
    throw std::runtime_error("Failed to create topic:" + topic_name);
  } else {
    std::cout << "Succeeded to create topic:" << topic_name << std::endl;
  }
}
}  // namespace google::scp::core::test
