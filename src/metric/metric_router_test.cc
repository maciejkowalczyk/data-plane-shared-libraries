//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "src/metric/metric_router.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"

namespace privacy_sandbox::server_common::metrics {

namespace metric_sdk = ::opentelemetry::sdk::metrics;
namespace metrics_api = ::opentelemetry::metrics;

using ::testing::ContainsRegex;

constexpr int kExportIntervalMillis = 100;

constexpr Definition<int, Privacy::kNonImpacting, Instrument::kUpDownCounter>
    kSafeCounter("safe_counter", "description");
constexpr Definition<double, Privacy::kNonImpacting, Instrument::kUpDownCounter>
    kSafeCounterDouble("safe_double_counter", "description");

constexpr double histogram_boundaries[] = {50, 100, 200};
constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kSafeHistogram("safe_histogram", "description", histogram_boundaries);
constexpr Definition<double, Privacy::kNonImpacting, Instrument::kHistogram>
    kSafeHistogramDouble("safe_double_histogram", "description",
                         histogram_boundaries);

constexpr std::string_view buyer_public_partitions[] = {"buyer_1", "buyer_2",
                                                        "buyer_3"};
constexpr Definition<int, Privacy::kNonImpacting,
                     Instrument::kPartitionedCounter>
    kSafePartitioned("safe_partitioned_counter", "description", "buyer_name",
                     buyer_public_partitions);
constexpr Definition<double, Privacy::kNonImpacting,
                     Instrument::kPartitionedCounter>
    kSafePartitionedDouble("safe_partitioned_double_counter", "description",
                           "buyer_name_double", buyer_public_partitions);

class MetricRouterTest : public ::testing::Test {
 protected:
  std::unique_ptr<MetricRouter::MeterProvider> init() {
    auto provider = std::make_unique<metric_sdk::MeterProvider>();
    provider->AddMetricReader(
        std::make_unique<metric_sdk::PeriodicExportingMetricReader>(
            std::make_unique<
                opentelemetry::exporter::metrics::OStreamMetricExporter>(
                GetSs(), metric_sdk::AggregationTemporality::kCumulative),
            metric_sdk::PeriodicExportingMetricReaderOptions{
                /*export_interval_millis*/ std::chrono::milliseconds(
                    kExportIntervalMillis),
                /*export_timeout_millis*/ std::chrono::milliseconds(
                    kExportIntervalMillis / 2)}));
    return provider;
  }

  std::unique_ptr<telemetry::BuildDependentConfig> InitConfig(
      absl::Duration dp_export_interval) {
    telemetry::TelemetryConfig config_proto;
    config_proto.set_dp_export_interval_ms(dp_export_interval /
                                           absl::Milliseconds(1));
    config_proto.set_metric_export_interval_ms(dp_export_interval /
                                               absl::Milliseconds(1));
    return std::make_unique<telemetry::BuildDependentConfig>(config_proto);
  }

  void SetUp() override {
    test_instance_ = std::make_unique<MetricRouter>(
        init(), "not used name", "0.0.1", PrivacyBudget{0},
        InitConfig(absl::Minutes(5)));
  }

  static std::stringstream& GetSs() {
    // never destructed, outlive 'OStreamMetricExporter'
    static auto* ss = new std::stringstream();
    return *ss;
  }

  std::string ReadSs() {
    absl::SleepFor(absl::Milliseconds(kExportIntervalMillis * 5));
    // Shut down metric reader now to avoid concurrent access of Ss.
    { auto not_used = std::move(test_instance_); }
    std::string output = GetSs().str();
    GetSs().str("");
    return output;
  }

  std::unique_ptr<MetricRouter> test_instance_;
};

TEST_F(MetricRouterTest, LogSafeInt) {
  CHECK_OK(test_instance_->LogSafe(kSafeCounter, 123, ""));
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]+:[ \t]+safe_counter"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+123"));
}

TEST_F(MetricRouterTest, LogSafeIntTwice) {
  CHECK_OK(test_instance_->LogSafe(kSafeCounter, 123, ""));
  CHECK_OK(test_instance_->LogSafe(kSafeCounter, 123, ""));
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]+:[ \t]+safe_counter"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+246"));
}

TEST_F(MetricRouterTest, LogSafeDouble) {
  CHECK_OK(test_instance_->LogSafe(kSafeCounterDouble, 4.56, ""));
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]+:[ \t]+safe_double_counter"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+4.56"));
}

TEST_F(MetricRouterTest, LogSafeIntHistogram) {
  CHECK_OK(test_instance_->LogSafe(kSafeHistogram, 123, ""));
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]+:[ \t]+safe_histogram"));
  EXPECT_THAT(output, ContainsRegex("sum[ \t]+:[ \t]+123"));
  EXPECT_THAT(output, ContainsRegex("buckets[ \t]+:[ \t]+[[]50, 100, 200"));
}

TEST_F(MetricRouterTest, LogSafeDoubleHistogram) {
  CHECK_OK(test_instance_->LogSafe(kSafeHistogramDouble, 100.23, ""));
  std::string output = ReadSs();

  EXPECT_THAT(output, ContainsRegex(
                          "instrument name[ \t]+:[ \t]+safe_double_histogram"));
  EXPECT_THAT(output, ContainsRegex("sum[ \t]+:[ \t]+100.23"));
  EXPECT_THAT(output, ContainsRegex("buckets[ \t]+:[ \t]+[[]50, 100, 200"));
}

TEST_F(MetricRouterTest, LogSafeDoubleHistogramTwice) {
  CHECK_OK(test_instance_->LogSafe(kSafeHistogramDouble, 100.11, ""));
  CHECK_OK(test_instance_->LogSafe(kSafeHistogramDouble, 200.22, ""));
  std::string output = ReadSs();

  EXPECT_THAT(output, ContainsRegex(
                          "instrument name[ \t]+:[ \t]+safe_double_histogram"));
  EXPECT_THAT(output, ContainsRegex("sum[ \t]+:[ \t]+300.33"));
  EXPECT_THAT(output, ContainsRegex("buckets[ \t]+:[ \t]+[[]50, 100, 200"));
}

TEST_F(MetricRouterTest, LogTwoMetric) {
  CHECK_OK(test_instance_->LogSafe(kSafeCounter, 123, ""));
  CHECK_OK(test_instance_->LogSafe(kSafeHistogram, 456, ""));
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]+:[ \t]+safe_counter"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+123"));
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]+:[ \t]+safe_histogram"));
  EXPECT_THAT(output, ContainsRegex("sum[ \t]+:[ \t]+456"));
}

TEST_F(MetricRouterTest, LogSafePartitioned) {
  CHECK_OK(test_instance_->LogSafe(kSafePartitioned, 111, "buyer_1"));
  CHECK_OK(test_instance_->LogSafe(kSafePartitioned, 1000, "buyer_1"));
  CHECK_OK(test_instance_->LogSafe(kSafePartitioned, 22, "buyer_2"));
  std::string output = ReadSs();
  EXPECT_THAT(
      output,
      ContainsRegex("instrument name[ \t]+:[ \t]+safe_partitioned_counter"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+1111"));
  EXPECT_THAT(output, ContainsRegex("buyer_name[ \t]*:[ \t]*buyer_1"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+22"));
  EXPECT_THAT(output, ContainsRegex("buyer_name[ \t]*:[ \t]*buyer_2"));
}

TEST_F(MetricRouterTest, LogSafePartitionedDouble) {
  CHECK_OK(test_instance_->LogSafe(kSafePartitionedDouble, 3.21, "buyer_3"));
  std::string output = ReadSs();
  EXPECT_THAT(
      output,
      ContainsRegex(
          "instrument name[ \t]+:[ \t]+safe_partitioned_double_counter"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+3.21"));
  EXPECT_THAT(output, ContainsRegex("buyer_name_double[ \t]*:[ \t]*buyer_3"));
}

class MetricRouterDpNoNoiseTest : public MetricRouterTest {
 protected:
  void SetUp() override {
    test_instance_ = std::make_unique<MetricRouter>(
        init(), "not used name", "0.0.1", PrivacyBudget{1e10},
        InitConfig(kDpInterval));
  }

  absl::Duration kDpInterval = 5 * absl::Milliseconds(kExportIntervalMillis);
};

constexpr Definition<int, Privacy::kImpacting, Instrument::kPartitionedCounter>
    kUnsafePartitioned(/*name*/ "kUnsafePartitioned", "",
                       /*partition_type*/ "buyer_name",
                       /*max_partitions_contributed*/ 3,
                       /*public_partitions*/ buyer_public_partitions,
                       /*upper_bound*/ 2,
                       /*lower_bound*/ 0);

constexpr Definition<int, Privacy::kImpacting, Instrument::kHistogram>
    kUnSafeHistogram("unsafe_histogram", "description", histogram_boundaries,
                     10000, 0);

TEST_F(MetricRouterDpNoNoiseTest, LogPartitioned) {
  for (int i = 0; i < 100; ++i) {
    CHECK_OK(test_instance_->LogUnSafe(kUnsafePartitioned, 111, "buyer_1"));
    CHECK_OK(test_instance_->LogUnSafe(kUnsafePartitioned, 22, "buyer_2"));
  }

  absl::SleepFor(kDpInterval);
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]*:[ \t]*kUnsafePartitioned"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]*:[ \t]*200"));
  EXPECT_THAT(output, ContainsRegex("buyer_name[ \t]*:[ \t]*buyer_1"));
  EXPECT_THAT(output, ContainsRegex("buyer_name[ \t]*:[ \t]*buyer_2"));
  EXPECT_THAT(output, ContainsRegex("buyer_name[ \t]*:[ \t]*buyer_3"));
}

TEST_F(MetricRouterDpNoNoiseTest, LogHistogram) {
  for (int i = 0; i < 100; i += 10) {
    CHECK_OK(test_instance_->LogUnSafe(kUnSafeHistogram, i, ""));
  }

  absl::SleepFor(kDpInterval);
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]*:[ \t]*unsafe_histogram"));
  EXPECT_THAT(output, ContainsRegex("buckets[ \t]*:[ \t]*\\[50, 100, 200"));
  EXPECT_THAT(output, ContainsRegex("counts[ \t]*:[ \t]*\\[6, 4, 0, 0"));
}

class MetricRouterDpNoiseTest : public MetricRouterTest {
 protected:
  void SetUp() override {
    test_instance_ = std::make_unique<MetricRouter>(init(), "not used name",
                                                    "0.0.1", PrivacyBudget{0.5},
                                                    InitConfig(kDpInterval));
  }

  absl::Duration kDpInterval = 5 * absl::Milliseconds(kExportIntervalMillis);
};

TEST_F(MetricRouterDpNoiseTest, LogPartitioned) {
  for (int i = 0; i < 100; ++i) {
    // kUnsafePartitioned bounded to [0, 2]
    for (std::string_view buyer : buyer_public_partitions) {
      CHECK_OK(test_instance_->LogUnSafe(kUnsafePartitioned, 111, buyer));
    }
  }

  absl::SleepFor(kDpInterval);
  std::string output = ReadSs();
  EXPECT_THAT(output,
              ContainsRegex("instrument name[ \t]*:[ \t]*kUnsafePartitioned"));

  for (std::string_view buyer : buyer_public_partitions) {
    EXPECT_THAT(output,
                ContainsRegex(absl::StrCat("buyer_name[ \t]*:[ \t]*", buyer)));
  }

  std::regex r("value[ \t]*:[ \t]*([0-9]+)");
  std::smatch sm;
  std::vector<int> results;
  for (int i = 0; i < 3; ++i) {
    regex_search(output, sm, r);
    results.push_back(stoi(sm[1]));
    EXPECT_THAT((double)results.back(), testing::DoubleNear(200, 150));
    output = sm.suffix();
  }
  bool at_least_one_not_200 = false;
  for (int i : results) {
    if (i != 200) at_least_one_not_200 = true;
  }
  EXPECT_TRUE(at_least_one_not_200);
}

constexpr Definition<int, Privacy::kNonImpacting, Instrument::kGauge>
    kTestGauge(/*name*/
               "test_gauge", /*description*/ "test_gauge");

absl::flat_hash_map<std::string, double> TestFetch() {
  return {
      {"p1", 1},
      {"p2", 2},
  };
}

TEST_F(MetricRouterTest, AddObserverable) {
  CHECK_OK(test_instance_->AddObserverable(kTestGauge, TestFetch));
  std::string output = ReadSs();
  EXPECT_THAT(output, ContainsRegex("instrument name[ \t]*:[ \t]*test_gauge"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+1"));
  EXPECT_THAT(output, ContainsRegex("label[ \t]*:[ \t]*p1"));
  EXPECT_THAT(output, ContainsRegex("value[ \t]+:[ \t]+2"));
  EXPECT_THAT(output, ContainsRegex("label[ \t]*:[ \t]*p2"));
}

}  // namespace privacy_sandbox::server_common::metrics
