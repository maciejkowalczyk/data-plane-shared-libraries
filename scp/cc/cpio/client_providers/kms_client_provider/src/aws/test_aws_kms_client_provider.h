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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_SRC_AWS_TEST_AWS_KMS_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_SRC_AWS_TEST_AWS_KMS_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "scp/cc/cpio/client_providers/kms_client_provider/src/aws/nontee_aws_kms_client_provider.h"
#include "scp/cc/public/cpio/test/kms_client/test_aws_kms_client_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AwsKmsClientProvider
 */
class TestAwsKmsClientProvider : public NonteeAwsKmsClientProvider {
 public:
  explicit TestAwsKmsClientProvider(
      TestAwsKmsClientOptions options,
      RoleCredentialsProviderInterface* role_credentials_provider,
      core::AsyncExecutorInterface* io_async_executor)
      : NonteeAwsKmsClientProvider(role_credentials_provider,
                                   io_async_executor),
        test_options_(std::move(options)) {}

 protected:
  Aws::Client::ClientConfiguration CreateClientConfiguration(
      std::string_view region) noexcept override;

  TestAwsKmsClientOptions test_options_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_SRC_AWS_TEST_AWS_KMS_CLIENT_PROVIDER_H_