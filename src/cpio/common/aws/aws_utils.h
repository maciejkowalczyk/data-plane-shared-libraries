
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
#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

namespace google::scp::cpio::common {
/**
 * @brief Creates the Client Config object which can be used to create AWS
 * clients.
 *
 * @return ClientConfiguration client configuration.
 */
Aws::Client::ClientConfiguration CreateClientConfiguration(
    std::string_view region = "") noexcept;
}  // namespace google::scp::cpio::common
