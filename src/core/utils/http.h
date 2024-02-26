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

#ifndef CORE_UTILS_HTTP_H_
#define CORE_UTILS_HTTP_H_

#include <string>

#include "src/core/interface/http_client_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::utils {
/**
 * @brief Get the Escaped URI from a HTTP request
 * Combines the path and query (after being escaped) in request and returns it.
 *
 * @param request
 * @return ExecutionResultOr<std::string>
 */
ExecutionResultOr<std::string> GetEscapedUriWithQuery(
    const HttpRequest& request);
}  // namespace google::scp::core::utils

#endif  // CORE_UTILS_HTTP_H_
