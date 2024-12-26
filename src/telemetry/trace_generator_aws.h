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

#ifndef COMPONENTS_TELEMETRY_AWS_XRAY_ID_GENERATOR_H_
#define COMPONENTS_TELEMETRY_AWS_XRAY_ID_GENERATOR_H_

#include <functional>
#include <memory>

#include "absl/time/clock.h"
#include "opentelemetry/sdk/trace/id_generator.h"

namespace privacy_sandbox::server_common {
std::unique_ptr<opentelemetry::sdk::trace::IdGenerator> CreateXrayIdGenerator(
    std::function<absl::Time()> now_func = absl::Now);
}  // namespace privacy_sandbox::server_common

#endif  // COMPONENTS_TELEMETRY_AWS_XRAY_ID_GENERATOR_H_
