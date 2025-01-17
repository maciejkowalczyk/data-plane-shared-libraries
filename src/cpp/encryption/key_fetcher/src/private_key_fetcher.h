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

#ifndef SRC_CPP_ENCRYPTION_KEY_FETCHER_PRIVATE_KEY_FETCHER_H_
#define SRC_CPP_ENCRYPTION_KEY_FETCHER_PRIVATE_KEY_FETCHER_H_

#include <memory>
#include <vector>

#include <public/cpio/interface/type_def.h>

#include "absl/container/flat_hash_map.h"
#include "cc/public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "cc/public/cpio/interface/type_def.h"
#include "src/cpp/encryption/key_fetcher/interface/private_key_fetcher_interface.h"

namespace privacy_sandbox::server_common {

// Implementation of PrivateKeyFetcherInterface that fetches private keys from
// the Private Key Service, caching them in memory, and maintains only the keys
// fetched during a sliding window.
class PrivateKeyFetcher : public PrivateKeyFetcherInterface {
 public:
  // Initializes an instance of PrivateKeyFetcher. `private_key_client` is an
  // instance of PrivateKeyClientInterface for communicating with the Private
  // Key Service, and private keys are cached in memory for a `ttl` duration.
  PrivateKeyFetcher(
      std::unique_ptr<google::scp::cpio::PrivateKeyClientInterface>
          private_key_client,
      absl::Duration ttl);

  // Stops and terminates any resources used by the fetcher.
  ~PrivateKeyFetcher();

  // Blocking.
  // Calls the Private Key Service to fetch and store the private keys.
  // Refresh() will also clean up any keys older than the ttl.
  absl::Status Refresh() noexcept override;

  // Returns the corresponding PrivateKey, if present.
  std::optional<PrivateKey> GetKey(
      const google::scp::cpio::PublicPrivateKeyPairId& public_key_id) noexcept
      override;

 private:
  // PrivateKeyClient for fetching private keys from the Private Key Service.
  std::unique_ptr<google::scp::cpio::PrivateKeyClientInterface>
      private_key_client_;

  absl::Mutex mutex_;

  // Map of the key ID to PrivateKey.
  absl::flat_hash_map<google::scp::cpio::PublicPrivateKeyPairId, PrivateKey>
      private_keys_map_ ABSL_GUARDED_BY(mutex_);

  // TTL of cached PrivateKey entries in private_keys_map_.
  absl::Duration ttl_;
};

}  // namespace privacy_sandbox::server_common

#endif  // SRC_CPP_ENCRYPTION_KEY_FETCHER_PRIVATE_KEY_FETCHER_H_
