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

#include "test_aws_role_credentials_provider.h"

#include <memory>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "src/cpio/client_providers/role_credentials_provider/aws/aws_role_credentials_provider.h"
#include "src/cpio/common/aws/test_aws_utils.h"

using Aws::Client::ClientConfiguration;
using google::scp::core::AsyncExecutorInterface;
using google::scp::cpio::common::test::CreateTestClientConfiguration;

namespace google::scp::cpio::client_providers {
ClientConfiguration TestAwsRoleCredentialsProvider::CreateClientConfiguration(
    std::string_view region) noexcept {
  return CreateTestClientConfiguration(sts_endpoint_override_,
                                       std::string(region));
}
}  // namespace google::scp::cpio::client_providers
