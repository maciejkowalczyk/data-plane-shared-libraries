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
// See the License for the specific language governing per21missions and
// limitations under the License.

#include <vector>

#include <include/gmock/gmock-matchers.h>

#include "absl/status/statusor.h"
#include "include/gtest/gtest.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/cpp/encryption/key_fetcher/interface/public_key_fetcher_interface.h"

namespace privacy_sandbox::server_common {

// Implementation of PublicKeyFetcherInterface to be used for unit testing any
// classes that have an instance of PublicKeyFetcher as a dependency.
class MockPublicKeyFetcher : public PublicKeyFetcherInterface {
 public:
  virtual ~MockPublicKeyFetcher() = default;

  MOCK_METHOD(absl::Status, Refresh, (), (noexcept));

  MOCK_METHOD(std::vector<google::scp::cpio::PublicPrivateKeyPairId>, GetKeyIds,
              (), (noexcept));

  MOCK_METHOD(
      absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>,
      GetKey, (), (noexcept));
};

}  // namespace privacy_sandbox::server_common
