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

#include "roma_benchmark.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "core/common/time_provider/src/time_provider.h"

using google::scp::core::ExecutionResult;
using google::scp::roma::BatchExecute;
using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::Execute;
using google::scp::roma::InvocationRequestSharedInput;
using google::scp::roma::LoadCodeObj;
using google::scp::roma::ResponseObject;
using google::scp::roma::RomaInit;
using google::scp::roma::RomaStop;
using google::scp::roma::benchmark::BenchmarkMetrics;
using google::scp::roma::benchmark::InputsType;
using google::scp::roma::sandbox::constants::
    kExecutionMetricJsEngineCallDuration;
using google::scp::roma::sandbox::constants::
    kExecutionMetricSandboxedJsEngineCallDuration;
using google::scp::roma::sandbox::constants::kHandlerCallMetricJsEngineDuration;
using google::scp::roma::sandbox::constants::
    kInputParsingMetricJsEngineDuration;
using std::atomic;
using std::cout;
using std::endl;
using std::list;
using std::thread;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::placeholders::_1;

namespace {

const list<float> kPercentiles = {50, 90, 99, 99.99};

CodeObject CreateCodeObj(const std::string& code_string) {
  CodeObject code_obj;
  code_obj.id = "foo";
  code_obj.version_num = 1;
  if (!code_string.empty()) {
    code_obj.js = code_string;
  } else {
    code_obj.js = "function Handler() { return \"Hello world!\";}";
  }

  return code_obj;
}

std::string FormatWithCommas(int value) {
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << std::fixed << value;
  return ss.str();
}

std::string GenerateRandomString() {
  auto length = std::rand() % 9 + 1;
  std::string output;
  for (auto i = 0; i < length; i++) {
    output += 'a' + std::rand() % 26;
  }
  return output;
}

std::string GenerateRandomJsonString(size_t depth, size_t wide) {
  std::string output = "{";

  for (auto i = 0; i < wide; i++) {
    auto json_key = GenerateRandomString();
    output += "\"" + json_key + "\":";
    std::string value;
    if (depth == 1) {
      value = "\"" + GenerateRandomString() + "\"";
    } else {
      value = GenerateRandomJsonString(depth - 1, wide);
    }
    output += value;
    if (i != wide - 1) {
      output += ",";
    }
  }

  output += "}";
  return output;
}

InvocationRequestSharedInput CreateExecutionObj(InputsType type,
                                                size_t payload_size,
                                                size_t json_depth) {
  InvocationRequestSharedInput code_obj;
  code_obj.id = "foo";
  code_obj.version_num = 1;
  code_obj.handler_name = "Handler";

  if (type == InputsType::kNestedJsonString) {
    std::string inputs_string =
        GenerateRandomJsonString(json_depth, 1 /*elements in each layer*/);
    code_obj.input.push_back(std::make_shared<std::string>(inputs_string));
    std::cout << "\tinputs size in Byte: " << inputs_string.length()
              << "\n\tinputs JSON depth: " << json_depth << std::endl;
  } else {
    std::string inputs_string(payload_size, 'A');
    code_obj.input.push_back(
        std::make_shared<std::string>("\"" + inputs_string + "\""));
    std::cout << "\tinputs size in Byte: " << inputs_string.length()
              << std::endl;
  }

  return code_obj;
}

void GetMetricFromResponse(const ResponseObject& resp,
                           BenchmarkMetrics& metrics) {
  if (const auto& it =
          resp.metrics.find(kExecutionMetricSandboxedJsEngineCallDuration);
      it != resp.metrics.end()) {
    metrics.sandbox_elapsed = it->second;
  }

  if (const auto& it = resp.metrics.find(kExecutionMetricJsEngineCallDuration);
      it != resp.metrics.end()) {
    metrics.v8_elapsed = it->second;
  }

  if (const auto& it = resp.metrics.find(kInputParsingMetricJsEngineDuration);
      it != resp.metrics.end()) {
    metrics.input_parsing_elapsed = it->second;
  }

  if (const auto& it = resp.metrics.find(kHandlerCallMetricJsEngineDuration);
      it != resp.metrics.end()) {
    metrics.handler_calling_elapse = it->second;
  }
}
}  // namespace

namespace google::scp::roma::benchmark {

void RomaBenchmarkSuite(const TestConfiguration& test_configuration) {
  Config config;
  config.number_of_workers = test_configuration.workers;
  config.worker_queue_max_items = test_configuration.queue_size;
  config.sandbox_request_response_shared_buffer_size_mb = 16;
  auto status = RomaInit(config);
  if (!status.ok()) {
    cout << "RomaInit failed due to " << status.message() << endl;
    return;
  }

  cout << "\nRoma RunTest config:"
       << "\n\tworkers: " << test_configuration.workers
       << "\n\tqueue_size: " << test_configuration.queue_size
       << "\n\trequest_threads: " << test_configuration.request_threads
       << "\n\trequests per thread: " << test_configuration.requests_per_thread
       << "\n\tBatch size: " << test_configuration.batch_size << endl;

  status = LoadCodeObject(test_configuration.js_source_code);
  if (!status.ok()) {
    cout << "LoadCodeObject failed due to " << status.message() << endl;
    return;
  }

  auto test_execute_request = CreateExecutionObj(
      test_configuration.inputs_type, test_configuration.input_payload_in_byte,
      test_configuration.input_json_nested_depth);

  RomaBenchmark roma_benchmark(test_execute_request,
                               test_configuration.batch_size,
                               test_configuration.request_threads,
                               test_configuration.requests_per_thread);

  roma_benchmark.RunTest();

  roma_benchmark.ConsoleTestMetrics();

  status = RomaStop();
  if (!status.ok()) {
    cout << "RomaStop failed due to " << status.message() << endl;
  }
}

BenchmarkMetrics BenchmarkMetrics::GetMeanMetrics(
    const std::vector<BenchmarkMetrics>& metrics) {
  auto num_metrics = metrics.size();
  BenchmarkMetrics mean_metric;
  for (size_t i = 0; i < num_metrics; i++) {
    mean_metric.total_execute_time += metrics[i].total_execute_time;
    mean_metric.sandbox_elapsed += metrics[i].sandbox_elapsed;
    mean_metric.v8_elapsed += metrics[i].v8_elapsed;
    mean_metric.input_parsing_elapsed += metrics[i].input_parsing_elapsed;
    mean_metric.handler_calling_elapse += metrics[i].handler_calling_elapse;
  }

  mean_metric.total_execute_time /= num_metrics;
  mean_metric.sandbox_elapsed /= num_metrics;
  mean_metric.v8_elapsed /= num_metrics;
  mean_metric.input_parsing_elapsed /= num_metrics;
  mean_metric.handler_calling_elapse /= num_metrics;

  return mean_metric;
}

absl::Status LoadCodeObject(const std::string& code_string) {
  // Loads code object to Roma workers.
  auto code_obj = CreateCodeObj(code_string);
  std::promise<void> done;
  std::atomic_bool load_success{false};
  auto status =
      LoadCodeObj(std::make_unique<CodeObject>(code_obj),
                  [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                    if (resp->ok()) {
                      load_success = true;
                    } else {
                      cout << "LoadCodeObj failed with "
                           << resp->status().message() << endl;
                    }
                    done.set_value();
                  });

  if (!status.ok()) {
    return status;
  }

  done.get_future().get();
  if (load_success) {
    return absl::OkStatus();
  } else {
    return absl::Status(absl::StatusCode::kInternal,
                        "Roma failed to load code object ");
  }
}

RomaBenchmark::RomaBenchmark(const InvocationRequestSharedInput& test_request,
                             size_t batch_size, size_t threads,
                             size_t requests_per_thread)
    : code_obj_(test_request),
      threads_(threads),
      batch_size_(batch_size),
      requests_per_thread_(requests_per_thread),
      latency_metrics_(threads * requests_per_thread, BenchmarkMetrics()) {}

void RomaBenchmark::RunTest() {
  privacy_sandbox::server_common::Stopwatch stopwatch;

  // Number of threads to send execute request.
  auto work_threads = std::vector<std::thread>();
  work_threads.reserve(threads_);
  for (auto i = 0; i < threads_; i++) {
    if (batch_size_ > 1) {
      work_threads.push_back(thread(&RomaBenchmark::SendRequestBatch, this));
    } else {
      work_threads.push_back(thread(&RomaBenchmark::SendRequest, this));
    }
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // wait until all requests got response.
  while (success_requests_ + failed_requests_ <
         threads_ * requests_per_thread_) {}
  elapsed_time_ = stopwatch.GetElapsedTime();
}

void RomaBenchmark::ConsoleTestMetrics() {
  auto empty_spots = threads_ * requests_per_thread_ - success_requests_;
  for (auto i = 0; i < empty_spots; i++) {
    latency_metrics_.pop_back();
  }
  cout << "\n Elapsed time: " << absl::ToInt64Nanoseconds(elapsed_time_)
       << " ns" << endl;
  cout << "\nNative Roma e2e total finished Requests: "
       << FormatWithCommas(success_requests_ + failed_requests_) << endl;
  cout << "Success Requests: " << FormatWithCommas(success_requests_) << endl;
  cout << "Failed Requests: " << FormatWithCommas(failed_requests_) << endl;

  cout << "RPS: "
       << FormatWithCommas((success_requests_ + failed_requests_) /
                           absl::ToInt64Seconds(elapsed_time_))
       << endl;

  auto average_metric = BenchmarkMetrics::GetMeanMetrics(latency_metrics_);
  cout << "\nMean metrics:" << endl;
  cout << "\te2e execution time: "
       << absl::ToInt64Nanoseconds(average_metric.total_execute_time) << " ns"
       << endl;
  cout << "\tSandbox elapsed: "
       << absl::ToInt64Nanoseconds(average_metric.sandbox_elapsed) << " ns"
       << endl;
  cout << "\tV8 elapsed: "
       << absl::ToInt64Nanoseconds(average_metric.v8_elapsed) << " ns" << endl;
  cout << "\tInput parsing elapsed: "
       << absl::ToInt64Nanoseconds(average_metric.input_parsing_elapsed)
       << " ns" << endl;
  cout << "\tHandler function calling elapsed: "
       << absl::ToInt64Nanoseconds(average_metric.handler_calling_elapse)
       << " ns\n"
       << endl;

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByTotalExec);
    cout << "e2e execution Elapsed: " << endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      cout << "\t" << p << "th percentile: "
           << absl::ToInt64Nanoseconds(
                  latency_metrics_.at(index).total_execute_time)
           << " ns" << endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareBySandboxElapsed);
    cout << "Sandbox Elapsed: " << endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      cout << "\t" << p << "th percentile: "
           << absl::ToInt64Nanoseconds(
                  latency_metrics_.at(index).sandbox_elapsed)
           << " ns" << endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByV8Elapsed);
    cout << "V8 Elapsed: " << endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      cout << "\t" << p << "th percentile: "
           << absl::ToInt64Nanoseconds(latency_metrics_.at(index).v8_elapsed)
           << " ns" << endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByInputsParsingElapsed);
    cout << "Inputs parsing Elapsed: " << endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      cout << "\t" << p << "th percentile: "
           << absl::ToInt64Nanoseconds(
                  latency_metrics_.at(index).input_parsing_elapsed)
           << " ns" << endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByHandlerCallingElapsed);
    cout << "Handler calling Elapsed: " << endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      cout << "\t" << p << "th percentile: "
           << absl::ToInt64Nanoseconds(
                  latency_metrics_.at(index).handler_calling_elapse)
           << " ns" << endl;
    }
  }
}

void RomaBenchmark::SendRequestBatch() {
  std::vector<InvocationRequestSharedInput> requests;
  for (auto i = 0; i < batch_size_; i++) {
    requests.push_back(code_obj_);
  }
  atomic<size_t> sent_request = 0;
  while (sent_request < requests_per_thread_) {
    while (!BatchExecute(requests,
                         bind(&RomaBenchmark::CallbackBatch, this, _1,
                              privacy_sandbox::server_common::Stopwatch()))
                .ok()) {}
    sent_request++;
  }
}

void RomaBenchmark::SendRequest() {
  atomic<size_t> sent_request = 0;
  while (sent_request < requests_per_thread_) {
    auto code_object =
        std::make_unique<InvocationRequestSharedInput>(code_obj_);
    // Retry Execute to dispatch code_obj until success.
    while (!Execute(move(code_object),
                    bind(&RomaBenchmark::Callback, this, _1,
                         privacy_sandbox::server_common::Stopwatch()))
                .ok()) {
      // Recreate code_object and update start_time when request send failed.
      code_object = std::make_unique<InvocationRequestSharedInput>(code_obj_);
    }
    sent_request++;
  }
}

void RomaBenchmark::CallbackBatch(
    const std::vector<absl::StatusOr<ResponseObject>> resp_batch,
    privacy_sandbox::server_common::Stopwatch stopwatch) {
  for (auto resp : resp_batch) {
    if (!resp.ok()) {
      failed_requests_.fetch_add(1);
      return;
    }
  }

  success_requests_.fetch_add(1);
  BenchmarkMetrics metric;
  metric.total_execute_time = stopwatch.GetElapsedTime();
  latency_metrics_.at(metric_index_) = metric;
  metric_index_.fetch_add(1);
}

void RomaBenchmark::Callback(
    std::unique_ptr<absl::StatusOr<ResponseObject>> resp,
    privacy_sandbox::server_common::Stopwatch stopwatch) {
  if (!resp->ok()) {
    failed_requests_.fetch_add(1);
    return;
  }
  success_requests_.fetch_add(1);

  BenchmarkMetrics metric;
  metric.total_execute_time = stopwatch.GetElapsedTime();
  GetMetricFromResponse(resp->value(), metric);
  latency_metrics_.at(metric_index_) = metric;
  metric_index_.fetch_add(1);
}

}  // namespace google::scp::roma::benchmark