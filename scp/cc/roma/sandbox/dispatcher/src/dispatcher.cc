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

#include "dispatcher.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "src/cpp/util/status_macro/status_macros.h"

using google::scp::core::errors::GetErrorMessage;

namespace google::scp::roma::sandbox::dispatcher {
absl::Status Dispatcher::Broadcast(std::unique_ptr<CodeObject> code_object,
                                   Callback broadcast_callback) noexcept {
  auto worker_count = worker_pool_->GetPoolSize();
  auto finished_counter = std::make_shared<std::atomic<size_t>>(0);
  auto responses_storage =
      std::make_shared<std::vector<absl::StatusOr<ResponseObject>>>(
          worker_count);

  auto broadcast_callback_ptr =
      std::make_shared<Callback>(std::move(broadcast_callback));
  for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
    auto callback = [worker_count, responses_storage, finished_counter,
                     broadcast_callback_ptr,
                     worker_index](absl::StatusOr<ResponseObject> response) {
      auto& all_resp = *responses_storage;
      // Store responses in the vector.
      all_resp[worker_index] = std::move(response);
      auto finished_value = finished_counter->fetch_add(1);
      // Go through the responses and call the callback on the first failed
      // one. If all succeeded, call the first callback.
      if (finished_value + 1 == worker_count) {
        for (auto& resp : all_resp) {
          if (!resp.ok()) {
            (*broadcast_callback_ptr)(std::move(resp));
            return;
          }
        }
        (*broadcast_callback_ptr)(std::move(all_resp[0]));
      }
    };

    auto code_object_copy = std::make_unique<CodeObject>(*code_object);
    PS_RETURN_IF_ERROR(Dispatch(std::move(code_object_copy),
                                std::move(callback), worker_index));
  }

  return absl::OkStatus();
}

absl::Status Dispatcher::ReloadCachedCodeObjects(
    worker_api::WorkerApi& worker) {
  const absl::flat_hash_map<std::string, CodeObject> all_cached_code_objects =
      [&] {
        absl::MutexLock l(&cache_mu_);
        return code_object_cache_.GetAll();
      }();

  {
    absl::MutexLock l(&pending_requests_mu_);
    pending_requests_ += all_cached_code_objects.size();
  }

  for (const auto& [_, cached_code] : all_cached_code_objects) {
    // TODO(b/317791484): Verify this is WAI.
    std::string request_type;
    if (!cached_code.wasm_bin.empty()) {
      request_type = std::string(constants::kRequestTypeJavascriptWithWasm);
    } else if (cached_code.js.empty()) {
      request_type = std::string(constants::kRequestTypeWasm);
    } else {
      request_type = std::string(constants::kRequestTypeJavascript);
    }
    const auto run_code_request =
        RequestConverter::FromUserProvided(cached_code, request_type);
    // Send the code objects to the worker again so it reloads its cache
    const auto run_code_result = worker.RunCode(run_code_request);
    if (!run_code_result.result().Successful()) {
      {
        absl::MutexLock l(&pending_requests_mu_);
        pending_requests_ -= all_cached_code_objects.size();
      }
      return absl::InternalError(
          absl::StrCat("Dispatcher RunCode failed due to: ",
                       google::scp::core::errors::GetErrorMessage(
                           run_code_result.result().status_code)));
    }
  }

  {
    absl::MutexLock l(&pending_requests_mu_);
    pending_requests_ -= all_cached_code_objects.size();
  }

  return absl::OkStatus();
}
}  // namespace google::scp::roma::sandbox::dispatcher
