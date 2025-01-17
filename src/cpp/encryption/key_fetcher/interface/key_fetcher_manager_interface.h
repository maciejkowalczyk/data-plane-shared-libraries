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

#ifndef SRC_CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_KEY_FETCHER_MANAGER_INTERFACE_H_
#define SRC_CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_KEY_FETCHER_MANAGER_INTERFACE_H_

#include <memory>

#include "absl/status/statusor.h"
#include "cc/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "cc/public/cpio/interface/type_def.h"
#include "src/cpp/concurrent/executor.h"
#include "src/cpp/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/cpp/encryption/key_fetcher/interface/public_key_fetcher_interface.h"

namespace privacy_sandbox::server_common {

// Interface responsible for returning public/private keys for cryptographic
// purposes.
class KeyFetcherManagerInterface {
 public:
  // Waits for any in-flight key fetch flows to complete and terminates any
  // resources used by the manager.
  virtual ~KeyFetcherManagerInterface() = default;

  // Fetches a public key to be used for encrypting outgoing requests.
  virtual absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetPublicKey() noexcept = 0;

  // Fetches the corresponding private key for a public key ID.
  virtual std::optional<PrivateKey> GetPrivateKey(
      const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept = 0;

  // Queues key refresh jobs on the class' executor as often as defined by
  // 'key_refresh_period'.
  virtual void Start() noexcept = 0;
};

// Factory to create KeyFetcherManager.
class KeyFetcherManagerFactory {
 public:
  // Creates a KeyFetcherManager given the Public and Private Key Fetchers and
  // an executor on which to run the periodic background key refresh job.
  static std::unique_ptr<KeyFetcherManagerInterface> Create(
      absl::Duration key_refresh_period,
      std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher,
      std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher,
      std::shared_ptr<Executor> executor);
};

}  // namespace privacy_sandbox::server_common

#endif  // SRC_CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_KEY_FETCHER_MANAGER_INTERFACE_H_
