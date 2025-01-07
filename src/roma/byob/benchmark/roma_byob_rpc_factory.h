// Copyright 2024 Google LLC
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

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/interface/roma_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

using AppService = ::privacy_sandbox::server_common::byob::RomaService<>;
using Config = ::privacy_sandbox::server_common::byob::Config<>;
using Mode = ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::SampleResponse;

namespace privacy_sandbox::server_common::byob {
using ExecutionFunc =
    absl::AnyInvocable<void(privacy_sandbox::server_common::Stopwatch,
                            absl::StatusOr<absl::Duration>*) const>;
using CleanupFunc = absl::AnyInvocable<void()>;

std::pair<ExecutionFunc, CleanupFunc> CreateByobRpcFunc(
    int num_workers, std::string_view lib_mounts, std::string_view binary_path,
    Mode mode, std::atomic<std::int64_t>& completions) {
  std::unique_ptr<AppService> roma_service = std::make_unique<AppService>();
  CHECK_OK(roma_service->Init(
      /*config=*/{.lib_mounts = std::string(lib_mounts)}, mode));

  absl::StatusOr<std::string> code_token =
      roma_service->LoadBinary(binary_path, num_workers);
  CHECK_OK(code_token);

  // Wait to make sure the workers are ready for work.
  absl::SleepFor(absl::Seconds(5));

  const auto rpc_func = [roma_service = roma_service.get(),
                         code_token = std::move(code_token), &completions](
                            privacy_sandbox::server_common::Stopwatch stopwatch,
                            absl::StatusOr<absl::Duration>* duration) {
    ::privacy_sandbox::roma_byob::example::SampleRequest request;
    request.set_function(FUNCTION_HELLO_WORLD);

    absl::StatusOr<google::scp::roma::ExecutionToken> exec_token =
        roma_service->ProcessRequest<SampleResponse>(
            std::string_view(*code_token), request,
            google::scp::roma::DefaultMetadata(),
            [stopwatch = std::move(stopwatch), duration,
             &completions](absl::StatusOr<SampleResponse> response) {
              *duration = stopwatch.GetElapsedTime();
              completions++;
              CHECK_OK(response);
            });
    if (!exec_token.ok()) {
      *duration = exec_token.status();
      completions++;
    }
  };

  auto callback = [roma_service = std::move(roma_service)]() mutable {
    LOG(INFO) << "Shutting down Roma";
    privacy_sandbox::server_common::Stopwatch stopwatch;
    roma_service.reset();
    LOG(INFO) << "Roma shutdown duration: " << stopwatch.GetElapsedTime();
  };

  return std::make_pair(std::move(rpc_func), std::move(callback));
}
}  // namespace privacy_sandbox::server_common::byob
