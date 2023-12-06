// Copyright 2023 Google LLC
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

#include <fcntl.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include <google/protobuf/text_format.h>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "core/common/time_provider/src/time_provider.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "scp/cc/public/cpio/validator/blob_storage_client_validator.h"
#include "scp/cc/public/cpio/validator/instance_client_validator.h"
#include "scp/cc/public/cpio/validator/parameter_client_validator.h"
#include "scp/cc/public/cpio/validator/proto/validator_config.pb.h"

namespace {

using google::scp::core::AsyncContext;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::validator::BlobStorageClientValidator;
using google::scp::cpio::validator::InstanceClientValidator;
using google::scp::cpio::validator::ParameterClientValidator;
using google::scp::cpio::validator::proto::ValidatorConfig;

constexpr std::string_view kRequestTimeout = "10";
const char kValidatorConfigPath[] = "/etc/validator_config.txtpb";
}  // namespace

std::string_view GetValidatorFailedToRunMsg() {
  return "FAILURE. Could not run all validation tests. For details, see above.";
}

google::scp::core::ExecutionResult MakeRequest(
    google::scp::core::HttpClientInterface& http_client,
    google::scp::core::HttpMethod method, const std::string& url,
    const absl::btree_multimap<std::string, std::string>& headers = {}) {
  auto request = std::make_shared<HttpRequest>();
  request->method = method;
  request->path = std::make_shared<std::string>(url);
  if (!headers.empty()) {
    request->headers =
        std::make_shared<google::scp::core::HttpHeaders>(headers);
    request->headers->insert({"Request-Timeout", std::string(kRequestTimeout)});
  }
  google::scp::core::ExecutionResult context_result;
  absl::Notification finished;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        context_result = context.result;
        finished.Notify();
      });

  if (auto result = http_client.PerformRequest(context); !result) {
    return result;
  }
  finished.WaitForNotification();
  return context_result;
}

void CheckProxy() {
  std::shared_ptr<google::scp::core::HttpClientInterface> http_client;
  auto res = GlobalCpio::GetGlobalCpio()->GetHttp1Client(http_client);
  if (!res) {
    std::cout << "FAILURE. Unable to get Http Client." << std::endl;
    return;
  }

  http_client->Init();
  http_client->Run();

  if (!MakeRequest(*http_client, google::scp::core::HttpMethod::GET,
                   "https://www.google.com/")) {
    std::cout << "FAILURE. Could not connect to outside world. Check if proxy "
                 "is running."
              << std::endl;
  } else {
    std::cout << "SUCCESS. Connected to outside world." << std::endl;
  }

  if (!MakeRequest(*http_client, google::scp::core::HttpMethod::PUT,
                   "http://169.254.169.254/latest/api/token",
                   {{"X-aws-ec2-metadata-token-ttl-seconds", "21600"}})) {
    std::cout
        << "FAILURE. Could not access AWS resource. Check if proxy is running."
        << std::endl;
  } else {
    std::cout << "SUCCESS. Accessed AWS resource." << std::endl;
  }

  http_client->Stop();
}

int main(int argc, char* argv[]) {
  // Process command line parameters
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  ValidatorConfig validator_config;
  int fd = open(kValidatorConfigPath, O_RDONLY);
  if (fd < 0) {
    std::cout << "FAILURE. Unable to open validator config file." << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  google::protobuf::io::FileInputStream file_input_stream(fd);
  file_input_stream.SetCloseOnDelete(true);

  if (!google::protobuf::TextFormat::Parse(&file_input_stream,
                                           &validator_config)) {
    std::cout << std::endl
              << "FAILURE. Unable to parse validator config file." << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  google::scp::cpio::CpioOptions cpio_options;
  cpio_options.log_option =
      (absl::StderrThreshold() == absl::LogSeverityAtLeast::kInfo)
          ? google::scp::cpio::LogOption::kConsoleLog
          : google::scp::cpio::LogOption::kNoLog;

  if (google::scp::core::ExecutionResult result =
          google::scp::cpio::Cpio::InitCpio(cpio_options);
      !result.Successful()) {
    std::cout << "FAILURE. Unable to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  CheckProxy();
  if (!validator_config.skip_instance_client_validation()) {
    InstanceClientValidator instance_client_validator;
    instance_client_validator.Run();
  }
  if (validator_config.has_parameter_client_config()) {
    ParameterClientValidator parameter_client_validator;
    parameter_client_validator.Run(validator_config.parameter_client_config());
  }
  if (validator_config.has_blob_storage_client_config()) {
    BlobStorageClientValidator blob_storage_client_validator;
    blob_storage_client_validator.Run(
        validator_config.blob_storage_client_config());
  }
  std::cout << "SUCCESS. Ran all validation tests. For individual statuses, "
               "see above."
            << std::endl;
  return 0;
}