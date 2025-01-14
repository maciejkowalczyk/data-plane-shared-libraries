/*
 * Copyright 2023 Google LLC
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

#ifndef SRC_CPP_ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_INTERFACE_H_
#define SRC_CPP_ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_INTERFACE_H_

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cc/public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace privacy_sandbox::server_common {

// Interface responsible for fetching and caching public keys.
class PublicKeyFetcherInterface {
 public:
  virtual ~PublicKeyFetcherInterface() = default;

  // Refreshes the fetcher's list of the latest public keys and, upon a
  // successful key fetch, invokes the callback passed into the method.
  virtual absl::Status Refresh() noexcept = 0;

  // Returns a public key for encrypting outgoing requests.
  virtual absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetKey() noexcept = 0;

  // Returns the IDs of the cached public keys.
  virtual std::vector<google::scp::cpio::PublicPrivateKeyPairId>
  GetKeyIds() noexcept = 0;
};

// Factory to create PublicKeyFetcher.
class PublicKeyFetcherFactory {
 public:
  // Creates a PublicKeyFetcher given a list of Public Key Service endpoints.
  static std::unique_ptr<PublicKeyFetcherInterface> Create(
      const std::vector<google::scp::cpio::PublicKeyVendingServiceEndpoint>&
          endpoints);
};

}  // namespace privacy_sandbox::server_common

#endif  // SRC_CPP_ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_INTERFACE_H_
