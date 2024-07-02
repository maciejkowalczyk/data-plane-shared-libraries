/*
 * Copyright 2024 Google LLC
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
 *
 * Example command to run this (the grep is necessary to avoid noisy log
 * output):
 *
 * builders/tools/bazel-debian run \
 * //src/roma/benchmark:isolate_restoration_benchmark \
 * --test_output=all 2>&1 | grep -Ev "sandbox.cc|monitor_base.cc|sandbox2.cc"
 */

#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace {

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::DefaultMetadata;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

constexpr std::string_view kHandlerName = "Handler";
constexpr absl::Duration kTimeout = absl::Seconds(10);

constexpr std::string_view kGlobalStructureUdfPathBase =
    "./src/roma/benchmark/global_structure_";
constexpr std::string_view kGlobalStringUdfPathBase =
    "./src/roma/benchmark/global_string_";

enum class GlobalType { Structure, String };

std::unique_ptr<RomaService<>> roma_service;

void DoTeardown(const ::benchmark::State& state) {
  CHECK_OK(roma_service->Stop());
  roma_service.reset();
}

void LoadCodeObj(std::string_view code) {
  absl::Notification load_finished;

  CHECK_OK(roma_service->LoadCodeObj(
      std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v1",
          .js = std::string(code),
      }),
      [&load_finished](const absl::StatusOr<ResponseObject>& resp) {
        CHECK_OK(resp);
        load_finished.Notify();
      }));

  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));
}

void DoSetup(const ::benchmark::State& state) {
  typename RomaService<>::Config config;
  config.number_of_workers = 2;

  roma_service.reset(new RomaService<>(std::move(config)));
  CHECK_OK(roma_service->Init());
}

std::string GetGlobalVariableUdf(int iter, GlobalType global_type) {
  std::string_view udf_path = global_type == GlobalType::Structure
                                  ? kGlobalStructureUdfPathBase
                                  : kGlobalStringUdfPathBase;
  std::ifstream inputFile(absl::StrCat(udf_path, iter, ".js"));
  std::string code((std::istreambuf_iterator<char>(inputFile)),
                   (std::istreambuf_iterator<char>()));
  CHECK(!code.empty());
  return code;
}

template <GlobalType T>
void BM_LoadGlobal(benchmark::State& state) {
  auto iter = state.range(0);
  const std::string code = GetGlobalVariableUdf(iter, T);
  for (auto _ : state) {
    LoadCodeObj(code);
  }
}

template <GlobalType T>
void BM_ExecuteGlobal(::benchmark::State& state) {
  auto iter = state.range(0);
  const std::string code = GetGlobalVariableUdf(iter, T);
  LoadCodeObj(code);
  for (auto _ : state) {
    absl::Notification execute_finished;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = std::string(kHandlerName),
        });
    CHECK_OK(roma_service->Execute(std::move(execution_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     CHECK_OK(resp);
                                     execute_finished.Notify();
                                   }));

    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
  }
}

BENCHMARK(BM_LoadGlobal<GlobalType::Structure>)
    ->Range(MIN_ITERATION, MAX_ITERATION)
    ->RangeMultiplier(8)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadGlobal<GlobalType::String>)
    ->Range(MIN_ITERATION, MAX_ITERATION)
    ->RangeMultiplier(8)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteGlobal<GlobalType::Structure>)
    ->Range(MIN_ITERATION, MAX_ITERATION)
    ->RangeMultiplier(8)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteGlobal<GlobalType::String>)
    ->Range(MIN_ITERATION, MAX_ITERATION)
    ->RangeMultiplier(8)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
}  // namespace

// Run the benchmark
BENCHMARK_MAIN();
