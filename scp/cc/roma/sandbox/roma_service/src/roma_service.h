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

#ifndef ROMA_SANDBOX_ROMA_SERVICE_SRC_ROMA_SERVICE_H_
#define ROMA_SANDBOX_ROMA_SERVICE_SRC_ROMA_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "core/async_executor/src/async_executor.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "roma/sandbox/dispatcher/src/dispatcher.h"
#include "roma/sandbox/native_function_binding/src/native_function_handler_sapi_ipc.h"
#include "roma/sandbox/native_function_binding/src/native_function_table.h"
#include "roma/sandbox/worker_pool/src/worker_pool.h"

#include "error_codes.h"

namespace google::scp::roma::sandbox::roma_service {
class RomaService : public core::ServiceInterface {
 public:
  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  /// The the instance of the roma service. This function is thread-unsafe when
  /// instance_ is null.
  static RomaService* Instance(const Config& config = Config())
      ABSL_LOCKS_EXCLUDED(instance_mu_) {
    absl::MutexLock lock(&instance_mu_);
    if (instance_ == nullptr) {
      instance_ = new RomaService(config);
    }
    return instance_;
  }

  static void Delete() ABSL_LOCKS_EXCLUDED(instance_mu_) {
    absl::MutexLock lock(&instance_mu_);
    if (instance_ != nullptr) {
      delete instance_;
      instance_ = nullptr;
    }
  }

  /// Return the dispatcher
  dispatcher::Dispatcher& Dispatcher() { return *dispatcher_; }

  RomaService(const RomaService&) = delete;
  RomaService() = default;

 private:
  struct NativeFunctionBindingSetup {
    std::vector<int> remote_file_descriptors;
    std::vector<int> local_file_descriptors;
    std::vector<std::string> js_function_names;
  };

  explicit RomaService(const Config& config = Config()) { config_ = config; }

  /**
   * @brief Setup the handler, create the socket pairs and return the sockets
   * that belongs to the sandbox side.
   * @param concurrency The number of processes to create resources for.
   *
   * @return A struct containing the remote function binding information
   */
  core::ExecutionResultOr<NativeFunctionBindingSetup>
  SetupNativeFunctionHandler(size_t concurrency);

  void RegisterLogBindings() noexcept;

  core::ExecutionResult SetupWorkers(
      const NativeFunctionBindingSetup& native_binding_setup);

  static absl::Mutex instance_mu_;
  static RomaService* instance_ ABSL_GUARDED_BY(instance_mu_);
  Config config_;
  std::unique_ptr<dispatcher::Dispatcher> dispatcher_;
  std::unique_ptr<worker_pool::WorkerPool> worker_pool_;
  std::unique_ptr<core::AsyncExecutor> async_executor_;
  native_function_binding::NativeFunctionTable native_function_binding_table_;
  std::shared_ptr<native_function_binding::NativeFunctionHandlerSapiIpc>
      native_function_binding_handler_;
};
}  // namespace google::scp::roma::sandbox::roma_service

#endif  // ROMA_SANDBOX_ROMA_SERVICE_SRC_ROMA_SERVICE_H_
