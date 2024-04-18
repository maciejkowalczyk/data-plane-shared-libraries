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
 *
 * Example command to run this (the grep is necessary to avoid noisy log
 * output):
 *
 * builders/tools/bazel-debian run \
 * //src/roma/benchmark:logging_benchmark \
 * --test_output=all 2>&1 | fgrep -v sandbox2.cc
 */

#include <memory>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "src/roma/benchmark/compiler/compiler_utils.h"
#include "src/roma/benchmark/test_code.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace google::scp::roma::benchmark::compiler {

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::DefaultMetadata;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::benchmark::kCodeHelloWorld;
using google::scp::roma::benchmark::kCodeJetstreamCryptoAes;
using google::scp::roma::benchmark::kCodeJetstreamDeltaBlue;
using google::scp::roma::benchmark::kCodeJetstreamNavierStokes;
using google::scp::roma::benchmark::kCodeJetstreamSplay;
using google::scp::roma::benchmark::kCodeJetstreamUniPoker;
using google::scp::roma::benchmark::kCodePrimeSieve;
using google::scp::roma::benchmark::kHandlerNameHelloWorld;
using google::scp::roma::benchmark::kHandlerNameJetstreamCryptoAes;
using google::scp::roma::benchmark::kHandlerNameJetstreamDeltaBlue;
using google::scp::roma::benchmark::kHandlerNameJetstreamNavierStokes;
using google::scp::roma::benchmark::kHandlerNameJetstreamSplay;
using google::scp::roma::benchmark::kHandlerNameJetstreamUniPoker;
using google::scp::roma::benchmark::kHandlerNamePrimeSieve;
using google::scp::roma::sandbox::roma_service::RomaService;

std::unique_ptr<RomaService<>> roma_service;

void DoSetup(typename RomaService<>::Config config) {
  roma_service.reset(new RomaService<>(std::move(config)));
  CHECK_OK(roma_service->Init());
}

void DoTeardown(const ::benchmark::State& state) {
  CHECK_OK(roma_service->Stop());
  roma_service.reset();
}

void LoadCodeObj(std::string_view code) {
  absl::Notification load_finished;

  absl::Status load_status = roma_service->LoadCodeObj(
      std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v1",
          .js = std::string(code),
      }),
      [&load_finished](const absl::StatusOr<ResponseObject>& resp) {
        CHECK(resp.ok());
        load_finished.Notify();
      });

  CHECK(load_status.ok()) << load_status;
  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));
}

void LoadCodeBenchmark(::benchmark::State& state, std::string_view code) {
  for (auto _ : state) {
    LoadCodeObj(code);
  }

  std::string label = absl::StrJoin(kOptimizerCombos[state.range(0)], " ");
  state.SetLabel(label.empty() ? "default" : label);
}

void SetupWithV8Flags(const ::benchmark::State& state) {
  typename RomaService<>::Config config;
  config.number_of_workers = 2;
  config.SetV8Flags(kOptimizerCombos[state.range(0)]);

  DoSetup(std::move(config));
}

void BM_LoadCodeObjHelloWorld(::benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeHelloWorld);
}

void BM_LoadCodeObjPrimeSieve(::benchmark::State& state) {
  LoadCodeBenchmark(state, kCodePrimeSieve);
}

void BM_LoadCodeObjJetstreamUniPoker(::benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamUniPoker);
}

void BM_LoadCodeObjJetstreamSplay(::benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamSplay);
}

void BM_LoadCodeObjJetstreamDeltaBlue(::benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamDeltaBlue);
}

void BM_LoadCodeObjJetstreamCryptoAes(::benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamCryptoAes);
}

void BM_LoadCodeObjJetstreamNavierStokes(::benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamNavierStokes);
}

BENCHMARK(BM_LoadCodeObjHelloWorld)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjPrimeSieve)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamUniPoker)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamSplay)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamDeltaBlue)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamCryptoAes)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamNavierStokes)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);

}  // namespace google::scp::roma::benchmark::compiler

// Run the benchmark
BENCHMARK_MAIN();
