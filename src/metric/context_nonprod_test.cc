//  Copyright 2023 Google LLC
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

#include "src/metric/context_test.h"

namespace privacy_sandbox::server_common::metrics {

using ::testing::Contains;

class ExperimentTest : public ContextTest {
 protected:
  void SetUp() override {
    InitConfig(telemetry::TelemetryConfig::EXPERIMENT);
    context_ = Context<metric_list_span, MockMetricRouter>::GetContext(
        &mock_metric_router_);
  }

  void ExpectCallLogSafe() {
    EXPECT_CALL(
        mock_metric_router_,
        LogSafe(Matcher<const DefinitionUnSafe&>(Ref(kIntApproximateCounter)),
                Eq(1), _, Contains(Pair(kNoiseAttribute, "Raw"))))
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(
        mock_metric_router_,
        LogSafe(Matcher<const DefinitionUnSafe&>(Ref(kIntApproximateCounter2)),
                Eq(2), _, Contains(Pair(kNoiseAttribute, "Raw"))))
        .WillOnce(Return(absl::OkStatus()));
  }
};

TEST_F(ExperimentTest, LogAfterDecrypt) {
  context_->SetDecrypted();
  ErrorLogSafeAfterDecrypt();
  ExpectCallLogSafe();
  CHECK_OK(context_->LogMetric<kIntApproximateCounter>(1));
  CHECK_OK(context_->LogMetricDeferred<kIntApproximateCounter2>(
      []() mutable { return 2; }));
}

class CompareTest : public ExperimentTest {
 protected:
  void SetUp() override {
    InitConfig(telemetry::TelemetryConfig::COMPARE);
    context_ = Context<metric_list_span, MockMetricRouter>::GetContext(
        &mock_metric_router_);
  }
};

TEST_F(CompareTest, LogBeforeDecrypt) { LogSafeOK(); }

TEST_F(CompareTest, LogAfterDecrypt) {
  context_->SetDecrypted();
  ErrorLogSafeAfterDecrypt();
  ExpectCallLogSafe();
  LogUnSafeForApproximate();
}

}  // namespace privacy_sandbox::server_common::metrics
