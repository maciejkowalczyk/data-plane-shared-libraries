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

#ifndef CORE_TEST_UTILS_HTTP1_HELPER_ERRORS_H_
#define CORE_TEST_UTILS_HTTP1_HELPER_ERRORS_H_

#include "src/core/interface/errors.h"

namespace google::scp::core::errors {

REGISTER_COMPONENT_CODE(SC_TEST_HTTP1_SERVER, 0x000F);

DEFINE_ERROR_CODE(SC_TEST_HTTP1_SERVER_ERROR_GETTING_SOCKET,
                  SC_TEST_HTTP1_SERVER, 0x0001,
                  "Http1Server error getting socket.",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_TEST_HTTP1_SERVER_ERROR_BINDING, SC_TEST_HTTP1_SERVER,
                  0x0002, "Http1Server error binding to port.",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_TEST_HTTP1_SERVER_ERROR_GETTING_SOCKET_NAME,
                  SC_TEST_HTTP1_SERVER, 0x0003,
                  "Http1Server error getting socket name.",
                  HttpStatusCode::BAD_REQUEST);

}  // namespace google::scp::core::errors

#endif  // CORE_TEST_UTILS_HTTP1_HELPER_ERRORS_H_
