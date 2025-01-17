// Copyright 2023 Google LLC
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

#include "src/cpp/encryption/key_fetcher/src/private_key_fetcher.h"

#include <utility>

#include "absl/strings/escaping.h"
#include "absl/time/clock.h"
#include "cc/public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "gmock/gmock.h"
#include "include/gtest/gtest.h"
#include "proto/hpke.pb.h"
#include "proto/tink.pb.h"
#include "public/core/interface/execution_result.h"

// Note: PKS = Private Key Service.
namespace privacy_sandbox::server_common {
namespace {

constexpr char kPublicKey[] = "pubkey";
constexpr char kPrivateKey[] = "privkey";

using ::google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using ::google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::cpio::Callback;
using ::google::scp::cpio::PrivateKeyClientInterface;
using ::google::scp::cpio::PublicPrivateKeyPairId;
using ::testing::Return;

class MockPrivateKeyClient
    : public google::scp::cpio::PrivateKeyClientInterface {
 public:
  ExecutionResult init_result_mock = SuccessExecutionResult();
  ExecutionResult Init() noexcept override { return init_result_mock; }

  ExecutionResult run_result_mock = SuccessExecutionResult();
  ExecutionResult Run() noexcept override { return run_result_mock; }

  ExecutionResult stop_result_mock = SuccessExecutionResult();
  ExecutionResult Stop() noexcept override { return stop_result_mock; }

  MOCK_METHOD(
      ExecutionResult, ListPrivateKeys,
      (google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest
           request,
       google::scp::cpio::Callback<
           google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>
           callback),
      (noexcept));
};

google::cmrt::sdk::private_key_service::v1::PrivateKey CreateFakePrivateKey(
    absl::string_view private_key, absl::string_view public_key,
    absl::string_view key_id) {
  google::crypto::tink::HpkePrivateKey hpke_private_key;
  hpke_private_key.set_private_key(private_key);

  google::crypto::tink::Keyset keyset;
  keyset.add_key()->mutable_key_data()->set_value(
      hpke_private_key.SerializeAsString());
  google::cmrt::sdk::private_key_service::v1::PrivateKey output;
  output.set_key_id(key_id);
  output.set_public_key(public_key);
  output.set_private_key(absl::Base64Escape(keyset.SerializeAsString()));
  output.mutable_creation_time()->set_seconds(ToUnixSeconds(absl::Now()));
  return output;
}

TEST(PrivateKeyFetcherTest, SuccessfulRefresh_SuccessfulPKSCall) {
  std::unique_ptr<MockPrivateKeyClient> mock_private_key_client =
      std::make_unique<MockPrivateKeyClient>();

  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(
      CreateFakePrivateKey(kPrivateKey, kPublicKey, "FF0000000"));

  EXPECT_CALL(*mock_private_key_client, ListPrivateKeys)
      .WillOnce(
          [&](ListPrivateKeysRequest request,
              Callback<ListPrivateKeysResponse> callback) -> ExecutionResult {
            // We pass 1 hour as the TTL below when we construct the fetcher.
            // For the first fetch, we should not be fetching all keys, not just
            // the ones passed into the method.
            EXPECT_EQ(request.max_age_seconds(),
                      ToInt64Seconds(absl::Hours(1)));
            EXPECT_EQ(0, request.key_ids().size());
            callback(SuccessExecutionResult(), response);
            return SuccessExecutionResult();
          });

  PrivateKeyFetcher fetcher(std::move(mock_private_key_client), absl::Hours(1));
  fetcher.Refresh();

  // Verify all fields were initialized correctly.
  EXPECT_TRUE(fetcher.GetKey("255").has_value());
  EXPECT_EQ(fetcher.GetKey("255")->key_id, "255");
  EXPECT_EQ(fetcher.GetKey("255")->private_key, kPrivateKey);
  EXPECT_TRUE(fetcher.GetKey("255")->creation_time - absl::Now() <
              absl::Minutes(1));
}

TEST(PrivateKeyFetcherTest,
     SuccessfulRefreshAndCleansOldKeys_SuccessfulPKSCall) {
  std::unique_ptr<MockPrivateKeyClient> mock_private_key_client =
      std::make_unique<MockPrivateKeyClient>();

  // The key fetcher will save the private key on the first refresh and clear
  // it out on the second refresh.
  EXPECT_CALL(*mock_private_key_client, ListPrivateKeys)
      .WillOnce(
          [&](ListPrivateKeysRequest request,
              Callback<ListPrivateKeysResponse> callback) -> ExecutionResult {
            ListPrivateKeysResponse response;
            response.mutable_private_keys()->Add(
                CreateFakePrivateKey(kPrivateKey, kPublicKey, "000000"));

            callback(SuccessExecutionResult(), response);
            return SuccessExecutionResult();
          })
      .WillOnce(
          [&](ListPrivateKeysRequest request,
              Callback<ListPrivateKeysResponse> callback) -> ExecutionResult {
            callback(SuccessExecutionResult(), ListPrivateKeysResponse());
            return SuccessExecutionResult();
          });

  PrivateKeyFetcher fetcher(std::move(mock_private_key_client),
                            absl::Nanoseconds(1));
  // TTL is 1 nanosecond and we wait 1 millisecond to refresh, so the key is
  // booted from the cache.
  fetcher.Refresh();
  absl::SleepFor(absl::Milliseconds(1));
  fetcher.Refresh();

  EXPECT_FALSE(fetcher.GetKey("000000").has_value());
}

TEST(PrivateKeyFetcherTest, UnsuccessfulSyncPKSCall_CleansOldKeys) {
  std::unique_ptr<MockPrivateKeyClient> mock_private_key_client =
      std::make_unique<MockPrivateKeyClient>();

  // The key fetcher will save the private key on the first refresh and clear
  // it out on the second refresh.
  EXPECT_CALL(*mock_private_key_client, ListPrivateKeys)
      .WillOnce(
          [&](ListPrivateKeysRequest request,
              Callback<ListPrivateKeysResponse> callback) -> ExecutionResult {
            ListPrivateKeysResponse response;
            response.mutable_private_keys()->Add(
                CreateFakePrivateKey(kPrivateKey, kPublicKey, "000000"));
            callback(SuccessExecutionResult(), response);
            return SuccessExecutionResult();
          })
      .WillOnce(
          [&](ListPrivateKeysRequest request,
              Callback<ListPrivateKeysResponse> callback) -> ExecutionResult {
            callback(FailureExecutionResult(0), ListPrivateKeysResponse());
            return FailureExecutionResult(0);
          });

  PrivateKeyFetcher fetcher(std::move(mock_private_key_client),
                            absl::Nanoseconds(1));
  // TTL is 1 nanosecond and we wait 1 millisecond to refresh, so the key is
  // booted from the cache.
  fetcher.Refresh();
  absl::SleepFor(absl::Milliseconds(1));
  fetcher.Refresh();

  EXPECT_FALSE(fetcher.GetKey("000000").has_value());
}

}  // namespace
}  // namespace privacy_sandbox::server_common
