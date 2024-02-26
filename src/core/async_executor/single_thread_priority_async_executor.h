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

#ifndef CORE_ASYNC_EXECUTOR_SINGLE_THREAD_PRIORITY_ASYNC_EXECUTOR_H_
#define CORE_ASYNC_EXECUTOR_SINGLE_THREAD_PRIORITY_ASYNC_EXECUTOR_H_

#include <memory>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "src/core/interface/async_executor_interface.h"

#include "async_task.h"

namespace google::scp::core {
/**
 * @brief A single threaded priority async executor. This executor will have one
 * thread working with one priority queue.
 */
class SingleThreadPriorityAsyncExecutor : ServiceInterface {
 public:
  explicit SingleThreadPriorityAsyncExecutor(
      size_t queue_cap,
      std::optional<size_t> affinity_cpu_number = std::nullopt)
      : is_running_(false),
        worker_thread_started_(false),
        worker_thread_stopped_(false),
        update_wait_time_(false),
        next_scheduled_task_timestamp_(UINT64_MAX),
        queue_cap_(queue_cap),
        affinity_cpu_number_(affinity_cpu_number) {}

  ExecutionResult Init() noexcept override ABSL_LOCKS_EXCLUDED(mutex_);

  ExecutionResult Run() noexcept override ABSL_LOCKS_EXCLUDED(mutex_);

  ExecutionResult Stop() noexcept override ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * @brief Schedules a task to be executed at a certain time.
   *
   * @param work The task that needs to be scheduled.
   * @param timestamp The timestamp to the task to be executed.
   * @return ExecutionResult The result of the execution with possible error
   * code.
   */
  ExecutionResult ScheduleFor(AsyncOperation work, Timestamp timestamp) noexcept
      ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * @brief Schedules a task to be executed at a certain time.
   *
   * @param work The task that needs to be scheduled.
   * @param timestamp The timestamp to the task to be executed.
   * @param cancellation_callback The callback to be used for cancelling the
   * scheduled work.
   * @return ExecutionResult result of the execution with possible error code.
   */
  ExecutionResult ScheduleFor(
      AsyncOperation work, Timestamp timestamp,
      std::function<bool()>& cancellation_callback) noexcept
      ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * @brief Returns the ID of the spawned thread object to enable looking it up
   * via thread IDs later. Will only be populated after Run() is called.
   */
  ExecutionResultOr<std::thread::id> GetThreadId() const
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  /// Starts the internal worker thread.
  void StartWorker() noexcept ABSL_LOCKS_EXCLUDED(mutex_);

  /**
   * @brief While it is true, the running thread will keep listening and
   * picking out work from work queue. While it is false, the thread will try to
   * finish all the remaining tasks in the queue and then stop.
   */
  bool is_running_ ABSL_GUARDED_BY(mutex_);
  /// Indicates whether the worker thread started.
  bool worker_thread_started_ ABSL_GUARDED_BY(mutex_);
  /// Indicates whether the worker thread stopped.
  bool worker_thread_stopped_ ABSL_GUARDED_BY(mutex_);
  /// Indicates whether the wait time needs to be updated.
  bool update_wait_time_ ABSL_GUARDED_BY(mutex_);
  /**
   * @brief The next scheduled task timestamp. This value helps with signaling
   * the thread at the next time of execution and prevents spin waiting.
   */
  Timestamp next_scheduled_task_timestamp_ ABSL_GUARDED_BY(mutex_);
  /// The maximum length of the work queue.
  size_t queue_cap_;
  /// An optional CPU to have an affinity for.
  std::optional<size_t> affinity_cpu_number_;
  /// A unique pointer to the working thread.
  std::optional<std::thread> working_thread_;
  /// The ID of the working_thread_.
  std::thread::id working_thread_id_;
  /// Queue for accepting the incoming tasks.
  /// TODO(b/315140459): Replace `priority_queue`. `shared_ptr` is necessary
  /// since `top` returns a const reference, necessitating a copy.
  std::optional<std::priority_queue<std::shared_ptr<AsyncTask>,
                                    std::vector<std::shared_ptr<AsyncTask>>,
                                    AsyncTaskCompareGreater>>
      queue_ ABSL_GUARDED_BY(mutex_);
  /**
   * @brief Used for signaling the thread that an element is pushed to the
   * queue.
   */
  mutable absl::Mutex mutex_;
};
}  // namespace google::scp::core

#endif  // CORE_ASYNC_EXECUTOR_SINGLE_THREAD_PRIORITY_ASYNC_EXECUTOR_H_
