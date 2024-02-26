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

#include "logging_utils.h"

#include <memory>
#include <utility>

#include "src/core/common/global_logger/global_logger.h"
#include "src/core/logger/interface/log_provider_interface.h"
#include "src/core/logger/log_providers/console_log_provider.h"
#include "src/core/logger/log_providers/syslog/syslog_log_provider.h"
#include "src/public/cpio/interface/type_def.h"

using google::scp::core::common::InitializeCpioLog;
using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::LogProviderInterface;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::cpio::LogOption;

namespace google::scp::core::test {

void TestLoggingUtils::EnableLogOutputToConsole() {
  InitializeCpioLog(LogOption::kConsoleLog);
}

void TestLoggingUtils::EnableLogOutputToSyslog() {
  InitializeCpioLog(LogOption::kSysLog);
}

};  // namespace google::scp::core::test
