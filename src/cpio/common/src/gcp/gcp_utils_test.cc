// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/cpio/common/src/gcp/gcp_utils.h"

#include <gtest/gtest.h>

#include <tuple>

#include "src/cpio/common/src/gcp/error_codes.h"
#include "src/public/core/src/test_execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_GCP_ABORTED;
using google::scp::core::errors::SC_GCP_ALREADY_EXISTS;
using google::scp::core::errors::SC_GCP_CANCELLED;
using google::scp::core::errors::SC_GCP_DATA_LOSS;
using google::scp::core::errors::SC_GCP_DEADLINE_EXCEEDED;
using google::scp::core::errors::SC_GCP_FAILED_PRECONDITION;
using google::scp::core::errors::SC_GCP_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::errors::SC_GCP_NOT_FOUND;
using google::scp::core::errors::SC_GCP_OUT_OF_RANGE;
using google::scp::core::errors::SC_GCP_PERMISSION_DENIED;
using google::scp::core::errors::SC_GCP_RESOURCE_EXHAUSTED;
using google::scp::core::errors::SC_GCP_UNAUTHENTICATED;
using google::scp::core::errors::SC_GCP_UNAVAILABLE;
using google::scp::core::errors::SC_GCP_UNIMPLEMENTED;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using ::testing::TestWithParam;

namespace google::scp::cpio::common::test {

class GcpUtilsCloudStatusTest
    : public TestWithParam<std::tuple<cloud::Status, ExecutionResult>> {
 protected:
  cloud::Status GetStatusToConvert() { return std::get<0>(GetParam()); }

  ExecutionResult GetExpectedResult() { return std::get<1>(GetParam()); }
};

TEST_P(GcpUtilsCloudStatusTest, GcpErrorConverter) {
  auto status = GetStatusToConvert();
  EXPECT_THAT(GcpUtils::GcpErrorConverter(status),
              ResultIs(GetExpectedResult()));
}

INSTANTIATE_TEST_SUITE_P(
    GcpUtilsCloudStatusTest, GcpUtilsCloudStatusTest,
    testing::Values(
        std::make_tuple(cloud::Status(cloud::StatusCode::kOk, ""),
                        SuccessExecutionResult()),
        std::make_tuple(cloud::Status(cloud::StatusCode::kNotFound, ""),
                        FailureExecutionResult(SC_GCP_NOT_FOUND)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kInvalidArgument, ""),
                        FailureExecutionResult(SC_GCP_INVALID_ARGUMENT)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kDeadlineExceeded, ""),
                        FailureExecutionResult(SC_GCP_DEADLINE_EXCEEDED)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kAlreadyExists, ""),
                        FailureExecutionResult(SC_GCP_ALREADY_EXISTS)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kUnimplemented, ""),
                        FailureExecutionResult(SC_GCP_UNIMPLEMENTED)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kOutOfRange, ""),
                        FailureExecutionResult(SC_GCP_OUT_OF_RANGE)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kCancelled, ""),
                        FailureExecutionResult(SC_GCP_CANCELLED)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kAborted, ""),
                        FailureExecutionResult(SC_GCP_ABORTED)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kUnavailable, ""),
                        FailureExecutionResult(SC_GCP_UNAVAILABLE)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kUnauthenticated, ""),
                        FailureExecutionResult(SC_GCP_UNAUTHENTICATED)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kPermissionDenied, ""),
                        FailureExecutionResult(SC_GCP_PERMISSION_DENIED)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kDataLoss, ""),
                        FailureExecutionResult(SC_GCP_DATA_LOSS)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kFailedPrecondition,
                                      ""),
                        FailureExecutionResult(SC_GCP_FAILED_PRECONDITION)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kResourceExhausted,
                                      ""),
                        FailureExecutionResult(SC_GCP_RESOURCE_EXHAUSTED)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kInternal, ""),
                        FailureExecutionResult(SC_GCP_INTERNAL_SERVICE_ERROR)),
        std::make_tuple(cloud::Status(cloud::StatusCode::kUnknown, ""),
                        FailureExecutionResult(SC_GCP_UNKNOWN))));

class GcpUtilsGrpcStatusTest
    : public TestWithParam<std::tuple<grpc::Status, ExecutionResult>> {
 protected:
  grpc::Status GetStatusToConvert() { return std::get<0>(GetParam()); }

  ExecutionResult GetExpectedResult() { return std::get<1>(GetParam()); }
};

TEST_P(GcpUtilsGrpcStatusTest, GcpErrorConverter) {
  auto status = GetStatusToConvert();
  EXPECT_THAT(GcpUtils::GcpErrorConverter(status),
              ResultIs(GetExpectedResult()));
}

INSTANTIATE_TEST_SUITE_P(
    GcpUtilsGrpcStatusTest, GcpUtilsGrpcStatusTest,
    testing::Values(
        std::make_tuple(grpc::Status(grpc::StatusCode::OK, ""),
                        SuccessExecutionResult()),
        std::make_tuple(grpc::Status(grpc::StatusCode::NOT_FOUND, ""),
                        FailureExecutionResult(SC_GCP_NOT_FOUND)),
        std::make_tuple(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, ""),
                        FailureExecutionResult(SC_GCP_INVALID_ARGUMENT)),
        std::make_tuple(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, ""),
                        FailureExecutionResult(SC_GCP_DEADLINE_EXCEEDED)),
        std::make_tuple(grpc::Status(grpc::StatusCode::ALREADY_EXISTS, ""),
                        FailureExecutionResult(SC_GCP_ALREADY_EXISTS)),
        std::make_tuple(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""),
                        FailureExecutionResult(SC_GCP_UNIMPLEMENTED)),
        std::make_tuple(grpc::Status(grpc::StatusCode::OUT_OF_RANGE, ""),
                        FailureExecutionResult(SC_GCP_OUT_OF_RANGE)),
        std::make_tuple(grpc::Status(grpc::StatusCode::CANCELLED, ""),
                        FailureExecutionResult(SC_GCP_CANCELLED)),
        std::make_tuple(grpc::Status(grpc::StatusCode::ABORTED, ""),
                        FailureExecutionResult(SC_GCP_ABORTED)),
        std::make_tuple(grpc::Status(grpc::StatusCode::UNAVAILABLE, ""),
                        FailureExecutionResult(SC_GCP_UNAVAILABLE)),
        std::make_tuple(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, ""),
                        FailureExecutionResult(SC_GCP_UNAUTHENTICATED)),
        std::make_tuple(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, ""),
                        FailureExecutionResult(SC_GCP_PERMISSION_DENIED)),
        std::make_tuple(grpc::Status(grpc::StatusCode::DATA_LOSS, ""),
                        FailureExecutionResult(SC_GCP_DATA_LOSS)),
        std::make_tuple(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, ""),
                        FailureExecutionResult(SC_GCP_FAILED_PRECONDITION)),
        std::make_tuple(grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, ""),
                        FailureExecutionResult(SC_GCP_RESOURCE_EXHAUSTED)),
        std::make_tuple(grpc::Status(grpc::StatusCode::INTERNAL, ""),
                        FailureExecutionResult(SC_GCP_INTERNAL_SERVICE_ERROR)),
        std::make_tuple(grpc::Status(grpc::StatusCode::UNKNOWN, ""),
                        FailureExecutionResult(SC_GCP_UNKNOWN))));

}  // namespace google::scp::cpio::common::test
