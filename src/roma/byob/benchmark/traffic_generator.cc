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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "src/roma/byob/benchmark/burst_generator.h"
#include "src/roma/byob/benchmark/roma_byob_rpc_factory.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/interface/roma_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/tools/v8_cli/roma_v8_rpc_factory.h"
#include "src/util/execution_token.h"
#include "src/util/periodic_closure.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(int, num_workers, 84, "Number of pre-created workers");
ABSL_FLAG(int, queries_per_second, 42,
          "Number of queries to be sent in a second");
ABSL_FLAG(int, burst_size, 14,
          "Number of times to call ProcessRequest for a single query");
ABSL_FLAG(int, num_queries, 10'000, "Number of queries to be sent");
ABSL_FLAG(privacy_sandbox::server_common::byob::Mode, sandbox,
          privacy_sandbox::server_common::byob::Mode::kModeSandbox,
          "Run BYOB in sandbox mode.");
ABSL_FLAG(std::string, lib_mounts, LIB_MOUNTS,
          "Mount paths to include in the pivot_root environment. Example "
          "/dir1,/dir2");
ABSL_FLAG(std::string, binary_path, "/udf/sample_udf", "Path to binary");
ABSL_FLAG(std::string, mode, "byob", "Traffic generator mode: 'byob' or 'v8'");
ABSL_FLAG(std::string, udf_path, "",
          "Path to JavaScript UDF file (V8 mode only)");
ABSL_FLAG(std::string, handler_name, "",
          "Name of the handler function to call (V8 mode only)");
ABSL_FLAG(std::vector<std::string>, input_args, {},
          "Arguments to pass to the handler function (V8 mode only)");

namespace {

using ::google::scp::roma::tools::v8_cli::CreateV8RpcFunc;
using ::privacy_sandbox::server_common::PeriodicClosure;

}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverity::kInfo);
  const int num_workers = absl::GetFlag(FLAGS_num_workers);
  CHECK_GT(num_workers, 0);
  const int num_queries = absl::GetFlag(FLAGS_num_queries);
  CHECK_GT(num_queries, 0);
  const int burst_size = absl::GetFlag(FLAGS_burst_size);
  CHECK_GT(burst_size, 0);
  const int queries_per_second = absl::GetFlag(FLAGS_queries_per_second);
  CHECK_GT(queries_per_second, 0);

  const std::string lib_mounts = absl::GetFlag(FLAGS_lib_mounts);
  const std::string binary_path = absl::GetFlag(FLAGS_binary_path);
  const privacy_sandbox::server_common::byob::Mode sandbox =
      absl::GetFlag(FLAGS_sandbox);

  const std::string udf_path = absl::GetFlag(FLAGS_udf_path);
  const std::string handler_name = absl::GetFlag(FLAGS_handler_name);
  const std::vector<std::string> input_args = absl::GetFlag(FLAGS_input_args);

  const std::string mode = absl::GetFlag(FLAGS_mode);
  CHECK(mode == "byob" || mode == "v8")
      << "Invalid mode. Must be 'byob' or 'v8'";

  absl::AnyInvocable<void()> stop_func;
  absl::AnyInvocable<void(privacy_sandbox::server_common::Stopwatch,
                          absl::StatusOr<absl::Duration>*) const>
      rpc_func;

  const std::int64_t expected_completions = num_queries * burst_size;
  std::atomic<std::int64_t> completions = 0;

  std::unique_ptr<PeriodicClosure> periodic = PeriodicClosure::Create();
  if (absl::Status s = periodic->StartDelayed(
          absl::Seconds(1),
          [&completions, expected_completions]() {
            static int64_t previous = 0;
            const int64_t curr_val = completions;
            if (previous != expected_completions) {
              LOG(INFO) << "completions: " << curr_val
                        << ", increment: " << curr_val - previous;
            }
            previous = curr_val;
          });
      !s.ok()) {
    LOG(FATAL) << s;
  }

  if (mode == "byob") {
    std::tie(rpc_func, stop_func) = CreateByobRpcFunc(
        num_workers, lib_mounts, binary_path, sandbox, completions);
  } else {  // v8 mode
    std::tie(rpc_func, stop_func) = CreateV8RpcFunc(
        num_workers, udf_path, handler_name, input_args, completions);
  }

  using ::privacy_sandbox::server_common::byob::BurstGenerator;
  const absl::Duration burst_cadence = absl::Seconds(1) / queries_per_second;
  BurstGenerator burst_gen("tg1", num_queries, burst_size, burst_cadence,
                           std::move(rpc_func));

  LOG(INFO) << "starting burst generator run."
            << "\n  burst size: " << burst_size
            << "\n  burst cadence: " << burst_cadence
            << "\n  num bursts: " << num_queries << std::endl;

  const BurstGenerator::Stats stats = burst_gen.Run();
  // RomaService must be cleaned up before stats are reported, to ensure the
  // service's work is completed
  stop_func();
  LOG(INFO) << "\n  burst size: " << burst_size
            << "\n  burst cadence: " << burst_cadence
            << "\n  num bursts: " << num_queries << std::endl;
  LOG(INFO) << stats.ToString() << std::endl;

  return stats.late_count == 0 ? 0 : 1;
}
