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

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "src/core/async_executor/src/async_executor.h"
#include "src/core/interface/service_interface.h"
#include "src/core/os/src/linux/system_resource_info_provider_linux.h"
#include "src/cpp/util/status_macro/status_macros.h"
#include "src/public/core/interface/execution_result.h"
#include "src/roma/logging/src/logging.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/dispatcher/src/dispatcher.h"
#include "src/roma/sandbox/native_function_binding/src/native_function_handler_sapi_ipc.h"
#include "src/roma/sandbox/native_function_binding/src/native_function_table.h"
#include "src/roma/sandbox/worker_api/src/worker_api_sapi.h"
#include "src/roma/sandbox/worker_pool/src/worker_pool_api_sapi.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::os::linux::SystemResourceInfoProviderLinux;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::sandbox::constants::kRequestUuid;
using google::scp::roma::sandbox::dispatcher::Dispatcher;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionHandlerSapiIpc;
using google::scp::roma::sandbox::native_function_binding::NativeFunctionTable;
using google::scp::roma::sandbox::worker_api::WorkerApiSapi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapiConfig;
using google::scp::roma::sandbox::worker_pool::WorkerPoolApiSapi;

namespace google::scp::roma::sandbox::roma_service {
constexpr int kWorkerQueueMax = 100;

// This value does not account for runtime memory usage and is only a generic
// estimate based on the memory needed by roma and the steady-state memory
// needed by v8.
constexpr uint64_t kDefaultMinStartupMemoryNeededPerWorkerKb = 400 * 1024;

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class RomaService {
 public:
  absl::Status Init() {
    if (!RomaHasEnoughMemoryForStartup()) {
      return absl::InternalError(
          "Roma startup failed due to insufficient system memory.");
    }
    PS_RETURN_IF_ERROR(InitInternal());
    PS_RETURN_IF_ERROR(RunInternal());
    return absl::OkStatus();
  }

  absl::Status LoadCodeObj(std::unique_ptr<CodeObject> code_object,
                           Callback callback) {
    if (code_object->version_string.empty()) {
      return absl::InvalidArgumentError(
          "Roma LoadCodeObj failed due to invalid version.");
    }
    return dispatcher_->Broadcast(std::move(code_object), std::move(callback));
  }

  // Async API.
  // Execute single invocation request. Can only be called when a valid
  // code object has been loaded.
  absl::Status Execute(
      std::unique_ptr<InvocationStrRequest<TMetadata>> invocation_req,
      Callback callback) {
    return ExecuteInternal(std::move(invocation_req), std::move(callback));
  }

  absl::Status Execute(
      std::unique_ptr<InvocationSharedRequest<TMetadata>> invocation_req,
      Callback callback) {
    return ExecuteInternal(std::move(invocation_req), std::move(callback));
  }

  absl::Status Execute(
      std::unique_ptr<InvocationStrViewRequest<TMetadata>> invocation_req,
      Callback callback) {
    return ExecuteInternal(std::move(invocation_req), std::move(callback));
  }

  // Async & Batch API.
  // Batch execute a batch of invocation requests. Can only be called when a
  // valid code object has been loaded.
  absl::Status BatchExecute(std::vector<InvocationStrRequest<>>& batch,
                            BatchCallback batch_callback) {
    return BatchExecuteInternal(batch, std::move(batch_callback));
  }

  absl::Status BatchExecute(std::vector<InvocationSharedRequest<>>& batch,
                            BatchCallback batch_callback) {
    return BatchExecuteInternal(batch, std::move(batch_callback));
  }

  absl::Status BatchExecute(std::vector<InvocationStrViewRequest<>>& batch,
                            BatchCallback batch_callback) {
    return BatchExecuteInternal(batch, std::move(batch_callback));
  }

  absl::Status Stop() { return StopInternal(); }

  RomaService(const RomaService&) = delete;

  /**
   * @brief The template parameter, TMetadata, needs to be default
   * assignable and movable.
   */
  explicit RomaService(Config<TMetadata>&& config = Config<TMetadata>())
      : config_(std::move(config)) {}

 private:
  absl::Status InitInternal() {
    size_t concurrency = config_.number_of_workers;
    if (concurrency == 0) {
      concurrency = std::thread::hardware_concurrency();
    }
    size_t worker_queue_cap = config_.worker_queue_max_items;
    if (worker_queue_cap == 0) {
      worker_queue_cap = kWorkerQueueMax;
    }

    RegisterLogBindings();
    PS_ASSIGN_OR_RETURN(auto native_function_binding_info,
                        SetupNativeFunctionHandler(concurrency));
    PS_RETURN_IF_ERROR(SetupWorkers(native_function_binding_info));

    async_executor_ =
        std::make_unique<AsyncExecutor>(concurrency, worker_queue_cap);
    auto execution_result = async_executor_->Init();
    if (!execution_result.Successful()) {
      return absl::InternalError(
          absl::StrCat("InitInternal failed due to internal error: ",
                       GetErrorMessage(execution_result.status_code)));
    }

    // TODO: Make max_pending_requests configurable
    dispatcher_ = std::make_unique<class Dispatcher>(
        async_executor_.get(), worker_pool_.get(),
        concurrency * worker_queue_cap /*max_pending_requests*/);
    ROMA_VLOG(1) << "RomaService Init with " << config_.number_of_workers
                 << " workers.";
    return absl::OkStatus();
  }

  absl::Status RunInternal() {
    native_function_binding_handler_->Run();
    if (auto execution_result = async_executor_->Run();
        !execution_result.Successful()) {
      return absl::InternalError(
          absl::StrCat("RunInternal failed due to internal error: ",
                       GetErrorMessage(execution_result.status_code)));
    }
    PS_RETURN_IF_ERROR(worker_pool_->Run());
    return absl::OkStatus();
  }

  absl::Status StopInternal() {
    if (native_function_binding_handler_) {
      native_function_binding_handler_->Stop();
    }
    native_function_binding_table_.Clear();
    if (worker_pool_) {
      PS_RETURN_IF_ERROR(worker_pool_->Stop());
    }
    if (async_executor_) {
      if (auto execution_result = async_executor_->Stop();
          !execution_result.Successful()) {
        return absl::InternalError(
            absl::StrCat("RunInternal failed due to internal error: ",
                         GetErrorMessage(execution_result.status_code)));
      }
    }
    return absl::OkStatus();
  }

  absl::Status StoreMetadata(std::string uuid, TMetadata metadata) {
    return native_function_binding_handler_->StoreMetadata(std::move(uuid),
                                                           std::move(metadata));
  }

  void DeleteMetadata(std::string_view uuid) {
    native_function_binding_handler_->DeleteMetadata(uuid);
  }

  struct NativeFunctionBindingSetup {
    std::vector<int> remote_file_descriptors;
    std::vector<int> local_file_descriptors;
    std::vector<std::string> js_function_names;
  };

  /**
   * @brief Setup the handler, create the socket pairs and return the sockets
   * that belongs to the sandbox side.
   * @param concurrency The number of processes to create resources for.
   *
   * @return A struct containing the remote function binding information
   */
  absl::StatusOr<NativeFunctionBindingSetup> SetupNativeFunctionHandler(
      size_t concurrency) {
    const auto function_bindings = config_.GetFunctionBindings();

    std::vector<std::string> function_names;
    function_names.reserve(function_bindings.size());
    for (const auto& binding : function_bindings) {
      PS_RETURN_IF_ERROR(native_function_binding_table_.Register(
          binding->function_name, binding->function));
      function_names.push_back(binding->function_name);
    }

    std::vector<int> local_fds;
    local_fds.reserve(concurrency);
    std::vector<int> remote_fds;
    remote_fds.reserve(concurrency);
    for (int i = 0; i < concurrency; i++) {
      int fd_pair[2];
      if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair) != 0) {
        return absl::InternalError(
            absl::StrCat("Failed to create socket for native function binding "
                         "communication."));
      }
      local_fds.push_back(fd_pair[0]);
      remote_fds.push_back(fd_pair[1]);
    }

    native_function_binding_handler_ =
        std::make_unique<NativeFunctionHandlerSapiIpc<TMetadata>>(
            &native_function_binding_table_, local_fds, remote_fds);

    NativeFunctionBindingSetup setup{
        .remote_file_descriptors = std::move(remote_fds),
        .local_file_descriptors = local_fds,
        .js_function_names = function_names,
    };
    return setup;
  }

  void RegisterLogBindings() {
    const auto log_fn_factory = [&](std::string_view function_name) {
      auto function_binding_object =
          std::make_unique<FunctionBindingObjectV2<TMetadata>>();
      function_binding_object->function_name = function_name;
      const auto severity = GetSeverity(function_name);
      function_binding_object->function =
          [severity, this](FunctionBindingPayload<TMetadata>& wrapper) {
            const auto& logging_func = config_.GetLoggingFunction();
            logging_func(severity, wrapper.metadata,
                         wrapper.io_proto.input_string());
            wrapper.io_proto.set_output_string("");
          };
      return function_binding_object;
    };
    for (const auto& name : {"ROMA_LOG", "ROMA_WARN", "ROMA_ERROR"}) {
      config_.RegisterFunctionBinding(log_fn_factory(name));
    }
  }

  absl::Status SetupWorkers(
      const NativeFunctionBindingSetup& native_binding_setup) {
    const auto& remote_fds = native_binding_setup.remote_file_descriptors;
    const auto& function_names = native_binding_setup.js_function_names;

    JsEngineResourceConstraints resource_constraints;
    config_.GetJsEngineResourceConstraints(resource_constraints);

    std::vector<WorkerApiSapiConfig> worker_configs;
    worker_configs.reserve(remote_fds.size());
    for (const int remote_fd : remote_fds) {
      WorkerApiSapiConfig worker_api_sapi_config{
          .js_engine_require_code_preload = true,
          .native_js_function_comms_fd = remote_fd,
          .native_js_function_names = function_names,
          .max_worker_virtual_memory_mb = config_.max_worker_virtual_memory_mb,
          .js_engine_resource_constraints = resource_constraints,
          .js_engine_max_wasm_memory_number_of_pages =
              config_.max_wasm_memory_number_of_pages,
          .sandbox_request_response_shared_buffer_size_mb =
              config_.sandbox_request_response_shared_buffer_size_mb,
          .enable_sandbox_sharing_request_response_with_buffer_only =
              config_.enable_sandbox_sharing_request_response_with_buffer_only,
      };
      worker_configs.push_back(worker_api_sapi_config);
    }
    worker_pool_ = std::make_unique<WorkerPoolApiSapi>(worker_configs);
    return worker_pool_->Init();
  }

  template <typename InputType>
  absl::Status AssertInvocationRequestIsValid(
      std::string_view function_name,
      const InvocationRequest<InputType, TMetadata>& invocation_req) {
    if (invocation_req.version_string.empty()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Roma ", function_name, " failed due to invalid version."));
    }

    if (invocation_req.handler_name.empty()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Roma ", function_name, " failed due to empty handler name."));
    }

    return absl::OkStatus();
  }

  template <typename RequestT>
  absl::Status ExecuteInternal(std::unique_ptr<RequestT> invocation_req,
                               Callback callback) {
    PS_RETURN_IF_ERROR(
        AssertInvocationRequestIsValid("Execute", *invocation_req));

    auto request_unique_id = google::scp::core::common::Uuid::GenerateUuid();
    std::string uuid_str =
        google::scp::core::common::ToString(request_unique_id);
    invocation_req->tags.insert(
        {std::string(google::scp::roma::sandbox::constants::kRequestUuid),
         uuid_str});

    invocation_req->tags.insert(
        {std::string(google::scp::roma::sandbox::constants::kMinLogLevel),
         absl::StrCat(invocation_req->min_log_level)});

    auto callback_ptr = std::make_unique<Callback>(std::move(callback));
    Callback callback_wrapper = [this, uuid_str,
                                 callback_ptr = std::move(callback_ptr)](
                                    absl::StatusOr<ResponseObject> resp) {
      (*callback_ptr)(std::move(resp));
      DeleteMetadata(uuid_str);
    };

    PS_RETURN_IF_ERROR(StoreMetadata(std::move(uuid_str),
                                     std::move(invocation_req->metadata)));
    return dispatcher_->Dispatch(std::move(invocation_req),
                                 std::move(callback_wrapper));
  }

  template <typename RequestT>
  absl::Status BatchExecuteInternal(std::vector<RequestT>& batch,
                                    BatchCallback batch_callback) {
    std::vector<std::string> uuids;
    uuids.reserve(batch.size());

    for (auto& request : batch) {
      PS_RETURN_IF_ERROR(
          AssertInvocationRequestIsValid("BatchExecute", request));
      auto request_unique_id = google::scp::core::common::Uuid::GenerateUuid();
      std::string uuid_str =
          google::scp::core::common::ToString(request_unique_id);
      // Save uuids for later removal in callback_wrapper
      uuids.push_back(uuid_str);
      request.tags.insert(
          {std::string(google::scp::roma::sandbox::constants::kRequestUuid),
           uuid_str});
      PS_RETURN_IF_ERROR(
          StoreMetadata(std::move(uuid_str), std::move(request.metadata)));
    }

    auto callback_ptr =
        std::make_unique<BatchCallback>(std::move(batch_callback));
    BatchCallback callback_wrapper =
        [&, uuids = std::move(uuids), callback_ptr = std::move(callback_ptr)](
            const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
          (*callback_ptr)(batch_resp);
          for (auto& uuid : uuids) {
            DeleteMetadata(uuid);
          }
        };

    return dispatcher_->DispatchBatch(batch, std::move(callback_wrapper));
  }

  bool RomaHasEnoughMemoryForStartup() {
    if (!config_.enable_startup_memory_check) {
      return true;
    }

    SystemResourceInfoProviderLinux mem_info;
    auto available_memory = mem_info.GetAvailableMemoryKb();
    ROMA_VLOG(1) << "Available memory is " << *available_memory << " Kb";
    if (!available_memory.result().Successful()) {
      // Failing to read the meminfo file should not stop startup.
      // This mem check is a best-effort check.
      return true;
    }

    if (config_.GetStartupMemoryCheckMinimumNeededValueKb) {
      return config_.GetStartupMemoryCheckMinimumNeededValueKb() <
             *available_memory;
    }

    auto cpu_count = std::thread::hardware_concurrency();
    auto num_processes = (config_.number_of_workers > 0 &&
                          config_.number_of_workers <= cpu_count)
                             ? config_.number_of_workers
                             : cpu_count;

    ROMA_VLOG(1) << "Number of workers is " << num_processes;

    auto minimum_memory_needed =
        num_processes * kDefaultMinStartupMemoryNeededPerWorkerKb;

    return minimum_memory_needed < *available_memory;
  }

  absl::LogSeverity GetSeverity(std::string_view severity) {
    if (severity == "ROMA_LOG") {
      return absl::LogSeverity::kInfo;
    } else if (severity == "ROMA_WARN") {
      return absl::LogSeverity::kWarning;
    } else {
      return absl::LogSeverity::kError;
    }
  }

  Config<TMetadata> config_;
  std::unique_ptr<dispatcher::Dispatcher> dispatcher_;
  std::unique_ptr<worker_pool::WorkerPool> worker_pool_;
  std::unique_ptr<core::AsyncExecutor> async_executor_;
  native_function_binding::NativeFunctionTable<TMetadata>
      native_function_binding_table_;
  std::shared_ptr<
      native_function_binding::NativeFunctionHandlerSapiIpc<TMetadata>>
      native_function_binding_handler_;
};
}  // namespace google::scp::roma::sandbox::roma_service

#endif  // ROMA_SANDBOX_ROMA_SERVICE_SRC_ROMA_SERVICE_H_