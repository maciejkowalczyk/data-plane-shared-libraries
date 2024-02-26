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

#include "test_aws_kms_client_provider.h"

#include <memory>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "src/cpio/client_providers/kms_client_provider/aws/nontee_aws_kms_client_provider.h"
#include "src/cpio/common/aws/test_aws_utils.h"
#include "src/public/cpio/interface/kms_client/type_def.h"
#include "src/public/cpio/test/kms_client/test_aws_kms_client_options.h"

using Aws::Client::ClientConfiguration;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::common::test::CreateTestClientConfiguration;

namespace google::scp::cpio::client_providers {
ClientConfiguration TestAwsKmsClientProvider::CreateClientConfiguration(
    std::string_view region) noexcept {
  return CreateTestClientConfiguration(*test_options_.kms_endpoint_override,
                                       std::string(region));
}
}  // namespace google::scp::cpio::client_providers
