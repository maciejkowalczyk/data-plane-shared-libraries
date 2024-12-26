/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aws_credentials_provider.h"

#include <memory>
#include <string>

#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core {

ExecutionResult AwsCredentialsProvider::Init() noexcept {
  return SuccessExecutionResult();
};

ExecutionResult AwsCredentialsProvider::GetCredentials(
    AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
        get_credentials_context) noexcept {
  ExecutionResult execution_result;
  try {
    auto aws_credentials = credentials_provider_->GetAWSCredentials();
    get_credentials_context.response =
        std::make_shared<GetCredentialsResponse>();
    get_credentials_context.response->access_key_id =
        std::make_shared<std::string>(
            aws_credentials.GetAWSAccessKeyId().c_str());
    get_credentials_context.response->access_key_secret =
        std::make_shared<std::string>(
            aws_credentials.GetAWSSecretKey().c_str());
    get_credentials_context.response->security_token =
        std::make_shared<std::string>(
            aws_credentials.GetSessionToken().c_str());
    execution_result = SuccessExecutionResult();
  } catch (...) {
    execution_result = FailureExecutionResult(
        core::errors::SC_CREDENTIALS_PROVIDER_FAILED_TO_FETCH_CREDENTIALS);
  }

  get_credentials_context.Finish(execution_result);
  return SuccessExecutionResult();
}

}  // namespace google::scp::core
