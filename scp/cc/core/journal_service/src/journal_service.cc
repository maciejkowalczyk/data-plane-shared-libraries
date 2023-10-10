/*
 * Copyright 2022 Google LLC
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

#include "journal_service.h"

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/metrics_def.h"
#include "core/journal_service/src/error_codes.h"
#include "core/journal_service/src/journal_input_stream.h"
#include "core/journal_service/src/journal_output_stream.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/src/metric_utils.h"
#include "public/cpio/utils/metric_aggregation/src/simple_metric.h"

using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalStreamAppendLogRequest;
using google::scp::core::journal_service::JournalStreamAppendLogResponse;
using google::scp::core::journal_service::JournalStreamReadLogRequest;
using google::scp::core::journal_service::JournalStreamReadLogResponse;
using google::scp::cpio::kDefaultMetricNamespace;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::MetricUtils;
using google::scp::cpio::SimpleMetricInterface;
using google::scp::cpio::TimeEvent;
using std::atomic;
using std::bind;
using std::function;
using std::make_pair;
using std::thread;
using std::unordered_set;
using std::chrono::milliseconds;
using std::placeholders::_1;
using std::this_thread::sleep_for;

static constexpr size_t kMaxWaitTimeForFlushMs = 20;

static constexpr size_t kStartupWaitIntervalMilliseconds = 100;

static constexpr char kJournalService[] = "JournalService";

namespace google::scp::core {

ExecutionResult JournalService::Init() noexcept {
  if (is_initialized_) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_ALREADY_INITIALIZED);
  }
  is_initialized_ = true;

  // Convert the UUID string to partition ID
  if (auto execution_result = FromString(*partition_name_, partition_id_);
      !execution_result.Successful()) {
    SCP_ERROR(kJournalService, kZeroUuid, execution_result,
              "Invalid partition name '%s'", partition_name_->c_str());
    return execution_result;
  }

  RETURN_IF_FAILURE(blob_storage_provider_->CreateBlobStorageClient(
      blob_storage_provider_client_));

  journal_input_stream_ = std::make_shared<JournalInputStream>(
      bucket_name_, partition_name_, blob_storage_provider_client_,
      config_provider_);

  auto recover_metric_labels =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kMetricComponentNameAndPartitionNamePrefixForJournalService +
              ToString(partition_id_),
          kMetricMethodRecover);
  auto recover_time_metric_info = MetricDefinition(
      kMetricNameRecoverExecutionTime, MetricUnit::kMilliseconds,
      kDefaultMetricNamespace, recover_metric_labels);
  recover_time_metric_ =
      metric_instance_factory_->ConstructSimpleMetricInstance(
          std::move(recover_time_metric_info));
  RETURN_IF_FAILURE(recover_time_metric_->Init());

  auto recover_count_metric_info = MetricDefinition(
      kMetricNameRecoverCount, MetricUnit::kCount, kDefaultMetricNamespace,
      std::move(recover_metric_labels));
  recover_log_count_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(recover_count_metric_info));
  RETURN_IF_FAILURE(recover_log_count_metric_->Init());

  auto output_metric_labels =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kMetricComponentNameAndPartitionNamePrefixForJournalService +
              ToString(partition_id_),
          kMetricMethodOutputStream);
  auto output_metric_info = MetricDefinition(
      kMetricNameJournalOutputStream, MetricUnit::kCount,
      kDefaultMetricNamespace, std::move(output_metric_labels));
  journal_output_count_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(output_metric_info),
          {kMetricEventJournalOutputCountWriteJournalScheduledCount,
           kMetricEventJournalOutputCountWriteJournalSuccessCount,
           kMetricEventJournalOutputCountWriteJournalFailureCount});

  RETURN_IF_FAILURE(journal_output_count_metric_->Init());

  if (!config_provider_
           ->Get(kJournalServiceFlushIntervalInMilliseconds,
                 journal_flush_interval_in_milliseconds_)
           .Successful()) {
    journal_flush_interval_in_milliseconds_ = kMaxWaitTimeForFlushMs;
  }

  SCP_INFO(
      kJournalService, partition_id_,
      "Starting Journal Service for Partition with ID: '%s'. Flush interval "
      "%zu milliseconds",
      ToString(partition_id_).c_str(), journal_flush_interval_in_milliseconds_);
  return SuccessExecutionResult();
}

ExecutionResult JournalService::Run() noexcept {
  if (!is_initialized_) {
    return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_NOT_INITIALIZED);
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_) {
      return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_RUNNING);
    }
    is_running_ = true;
  }

  RETURN_IF_FAILURE(journal_output_count_metric_->Run());

  atomic<bool> flushing_thread_started(false);
  flushing_thread_ =
      std::make_unique<thread>([this, &flushing_thread_started]() {
        flushing_thread_started = true;
        FlushJournalOutputStream();
      });

  while (!flushing_thread_started) {
    sleep_for(milliseconds(kStartupWaitIntervalMilliseconds));
  }

  return SuccessExecutionResult();
}

ExecutionResult JournalService::RunRecoveryMetrics() noexcept {
  RETURN_IF_FAILURE(recover_time_metric_->Run());
  RETURN_IF_FAILURE(recover_log_count_metric_->Run());
  return SuccessExecutionResult();
}

ExecutionResult JournalService::Stop() noexcept {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_running_) {
      return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_STOPPED);
    }
    is_running_ = false;
  }

  RETURN_IF_FAILURE(journal_output_count_metric_->Stop());

  if (flushing_thread_->joinable()) {
    flushing_thread_->join();
  }

  return SuccessExecutionResult();
}

ExecutionResult JournalService::StopRecoveryMetrics() noexcept {
  RETURN_IF_FAILURE(recover_time_metric_->Stop());
  RETURN_IF_FAILURE(recover_log_count_metric_->Stop());
  return SuccessExecutionResult();
}

ExecutionResult JournalService::Recover(
    AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
        journal_recover_context) noexcept {
  std::shared_ptr<TimeEvent> time_event = std::make_shared<TimeEvent>();
  auto replayed_log_ids = std::make_shared<unordered_set<std::string>>();
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context(
          std::make_shared<JournalStreamReadLogRequest>(),
          bind(&JournalService::OnJournalStreamReadLogCallback, this,
               time_event, replayed_log_ids, journal_recover_context, _1),
          journal_recover_context);
  journal_stream_read_log_context.request->max_journal_id_to_process =
      journal_recover_context.request->max_journal_id_to_process;
  journal_stream_read_log_context.request->max_number_of_journals_to_process =
      journal_recover_context.request->max_number_of_journals_to_process;
  journal_stream_read_log_context.request
      ->should_read_stream_when_only_checkpoint_exists =
      journal_recover_context.request
          ->should_perform_recovery_with_only_checkpoint_in_stream;

  SCP_INFO(kJournalService, partition_id_,
           "Starting JournalStreamReadLogRequest. Max journal id to process: "
           "'%llu', max number of journals to process: '%llu', Should perform "
           "recovery when "
           "there is only a checkpoint to process: '%d'",
           journal_stream_read_log_context.request->max_journal_id_to_process,
           journal_stream_read_log_context.request
               ->max_number_of_journals_to_process,
           journal_stream_read_log_context.request
               ->should_read_stream_when_only_checkpoint_exists);

  return journal_input_stream_->ReadLog(journal_stream_read_log_context);
}

void JournalService::OnJournalStreamReadLogCallback(
    std::shared_ptr<TimeEvent>& time_event,
    std::shared_ptr<unordered_set<std::string>>& replayed_log_ids,
    AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
        journal_recover_context,
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context) noexcept {
  if (!journal_stream_read_log_context.result.Successful()) {
    journal_recover_context.result = SuccessExecutionResult();
    if (journal_stream_read_log_context.result !=
        FailureExecutionResult(
            errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN)) {
      journal_recover_context.result = journal_stream_read_log_context.result;
    } else {
      time_event->Stop();
      recover_time_metric_->Push(std::to_string(time_event->diff_time));
      journal_recover_context.response =
          std::make_shared<JournalRecoverResponse>();
      journal_recover_context.response->last_processed_journal_id =
          journal_input_stream_->GetLastProcessedJournalId();
      journal_output_stream_ = std::make_shared<JournalOutputStream>(
          bucket_name_, partition_name_, async_executor_,
          blob_storage_provider_client_, journal_output_count_metric_);
      // Set to nullptr to deallocate the stream and its data.
      journal_input_stream_ = nullptr;
    }
    journal_recover_context.Finish();
    return;
  }

  SCP_INFO_CONTEXT(kJournalService, journal_recover_context,
                   "Replaying '%llu' number of logs",
                   journal_stream_read_log_context.response->read_logs->size());

  JournalId journal_id = kInvalidJournalId;
  size_t journal_log_counter = 0;

  for (const auto& log : *journal_stream_read_log_context.response->read_logs) {
    recover_log_count_metric_->Increment(kMetricEventNameLogCount);

    // Log the journal ID if not already logged.
    // Logs come in the journal buffer order. Log each time we switch journal
    // IDs.
    if (log.journal_id != journal_id) {
      if (journal_id != kInvalidJournalId) {
        SCP_INFO_CONTEXT(kJournalService, journal_recover_context,
                         "Replayed '%llu' logs from journal with ID: '%llu'",
                         journal_log_counter, journal_id);
      }
      SCP_INFO_CONTEXT(kJournalService, journal_recover_context,
                       "Replaying logs from journal with ID: '%llu'...",
                       log.journal_id);
      journal_log_counter = 0;
      journal_id = log.journal_id;
    }
    journal_log_counter++;

    OnLogRecoveredCallback callback;
    auto pair = make_pair(log.component_id, callback);
    auto execution_result = subscribers_map_.Find(log.component_id, callback);
    auto component_id_str = core::common::ToString(log.component_id);
    if (!execution_result.Successful()) {
      SCP_ERROR_CONTEXT(
          kJournalService, journal_recover_context, execution_result,
          "Cannot find the component with id %s", component_id_str.c_str());

      journal_recover_context.result = execution_result;
      journal_recover_context.Finish();
      return;
    }

    auto log_id_str = core::common::ToString(log.log_id);

    // Check to see if the logs has been already replayed. There is always a
    // chance that a retry call makes the same log again.
    auto log_index = absl::StrCat(component_id_str, "_", log_id_str);
    if (replayed_log_ids->find(log_index) != replayed_log_ids->end()) {
      SCP_DEBUG_CONTEXT(kJournalService, journal_recover_context,
                        "Duplicate log id: %s.", log_index.c_str());
      continue;
    }

    replayed_log_ids->emplace(log_index);

    auto bytes_buffer =
        std::make_shared<BytesBuffer>(log.journal_log->log_body());
    execution_result =
        callback(bytes_buffer, journal_recover_context.activity_id);
    if (!execution_result.Successful()) {
      SCP_ERROR_CONTEXT(
          kJournalService, journal_recover_context, execution_result,
          "Cannot handle the journal log with id %s for component id %s. "
          "Checkpoint/Journal ID where this came from: %llu",
          ToString(log.log_id).c_str(), component_id_str.c_str(),
          log.journal_id);

      journal_recover_context.result = execution_result;
      journal_recover_context.Finish();
      return;
    }
  }

  if (journal_id != kInvalidJournalId) {
    SCP_INFO_CONTEXT(kJournalService, journal_recover_context,
                     "Replayed '%llu' logs from journal with ID: '%llu'",
                     journal_log_counter, journal_id);
  }

  // There might be lots of logs to recover, there need to be a mechanism to
  // reduce the call stack size. Currently there is 1MB max stack limitation
  // that needed to be avoided.
  auto execution_result = async_executor_->Schedule(
      [operation_dispatcher = &operation_dispatcher_,
       journal_stream_read_log_context,
       journal_input_stream = journal_input_stream_]() mutable {
        operation_dispatcher->Dispatch<AsyncContext<
            JournalStreamReadLogRequest, JournalStreamReadLogResponse>>(
            journal_stream_read_log_context,
            [journal_input_stream](AsyncContext<JournalStreamReadLogRequest,
                                                JournalStreamReadLogResponse>&
                                       journal_stream_read_log_context) {
              return journal_input_stream->ReadLog(
                  journal_stream_read_log_context);
            });
      },
      AsyncPriority::Urgent);
  if (!execution_result.Successful()) {
    SCP_CRITICAL_CONTEXT(
        kJournalService, journal_recover_context, execution_result,
        "Cannot schedule AsyncTask for subsequent journal "
        "reads from the input stream. Returning a failure for the Recover().");
    journal_recover_context.result = execution_result;
    journal_recover_context.Finish();
  }
}

ExecutionResult JournalService::Log(
    AsyncContext<JournalLogRequest, JournalLogResponse>&
        journal_log_context) noexcept {
  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context(
          std::make_shared<JournalStreamAppendLogRequest>(),
          bind(&JournalService::OnJournalStreamAppendLogCallback, this,
               journal_log_context, _1),
          journal_log_context);
  journal_stream_append_log_context.request->journal_log =
      std::make_shared<JournalLog>();
  journal_stream_append_log_context.request->journal_log->set_log_body(
      journal_log_context.request->data->bytes->data(),
      journal_log_context.request->data->length);
  journal_stream_append_log_context.request->component_id =
      journal_log_context.request->component_id;
  journal_stream_append_log_context.request->log_id =
      journal_log_context.request->log_id;
  journal_stream_append_log_context.request->log_status =
      journal_log_context.request->log_status;

  return journal_output_stream_->AppendLog(journal_stream_append_log_context);
}

void JournalService::OnJournalStreamAppendLogCallback(
    AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
    AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>&
        journal_stream_append_log_context) noexcept {
  journal_log_context.result = journal_stream_append_log_context.result;
  journal_log_context.Finish();
}

ExecutionResult JournalService::SubscribeForRecovery(
    const Uuid& component_id, OnLogRecoveredCallback callback) noexcept {
  if (is_running()) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_CANNOT_SUBSCRIBE_WHEN_RUNNING);
  }

  auto pair = make_pair(component_id, callback);
  auto execution_result = subscribers_map_.Insert(pair, callback);
  return execution_result;
}

ExecutionResult JournalService::UnsubscribeForRecovery(
    const Uuid& component_id) noexcept {
  if (is_running()) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_CANNOT_UNSUBSCRIBE_WHEN_RUNNING);
  }

  auto id = component_id;
  return subscribers_map_.Erase(id);
}

ExecutionResult JournalService::GetLastPersistedJournalId(
    JournalId& journal_id) noexcept {
  if (journal_output_stream_ == nullptr) {
    return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_NO_OUTPUT_STREAM);
  }

  return journal_output_stream_->GetLastPersistedJournalId(journal_id);
}

void JournalService::FlushJournalOutputStream() noexcept {
  while (is_running()) {
    if (journal_output_stream_) {
      while (!journal_output_stream_->FlushLogs().Successful()) {}
    }

    sleep_for(milliseconds(journal_flush_interval_in_milliseconds_));
  }
}

}  // namespace google::scp::core