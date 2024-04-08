/*
 * Copyright 2024 Google LLC
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

#include <fstream>
#include <iostream>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::CodeObject;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

constexpr std::string_view kCommandsMessage =
    R"(
Shell Commands:

load - Load a User Defined Function (UDF)
Usage: load [VERSION_STR] [PATH_TO_UDF]
    Note: If PATH_TO_UDF is omitted, the UDF will be read from the command line.
Example: load v1 src/roma/tools/v8_cli/sample.js

execute - Execute a User Defined Function (UDF)
Usage: Execute [VERSION_STR] [UDF_NAME] [UDF_INPUT_ARGS]
Example: execute v1 HandleFunc foo bar

help - Display all shell commands
Usage: help

exit - Exit the tool
Usage: exit
)";

constexpr absl::Duration kRequestTimeout = absl::Seconds(10);

ABSL_FLAG(uint16_t, num_workers, 1, "Number of Roma workers");
ABSL_FLAG(bool, verbose, false, "Log all messages from shell");

// Get UDF from command line or input file if specified
std::string GetUDF(std::string_view udf_file_path) {
  std::string js;
  if (udf_file_path.empty()) {
    std::cout << "Please provide the JavaScript UDF. Press Enter to finish."
              << std::endl;
    std::string js_line;
    while (true) {
      if (!std::getline(std::cin, js_line) || js_line.empty()) {
        break;
      }
      absl::StrAppend(&js, js_line, "\n");
    }
    LOG(INFO) << js;
  } else {
    // Build Roma CodeOjbect from UDF code file.
    LOG(INFO) << "Loading UDF from file \"" << udf_file_path << "\"...";
    std::ifstream input_str(udf_file_path.data());
    std::string udf_js_code((std::istreambuf_iterator<char>(input_str)),
                            (std::istreambuf_iterator<char>()));
    js = udf_js_code;
  }
  return js;
}

void Load(RomaService<>* roma_service, std::string_view version_str,
          std::string_view udf_file_path) {
  std::string js = GetUDF(udf_file_path);
  if (js.empty()) {
    std::cout << "Empty UDF cannot be loaded. Please try again. " << std::endl;
    return;
  }
  auto uuid = google::scp::core::common::Uuid::GenerateUuid();
  std::string uuid_str = google::scp::core::common::ToString(uuid);

  const CodeObject code_object = {
      .id = uuid_str,
      .version_string = version_str.data(),
      .js = js.data(),
  };

  LOG(INFO) << "UDF JS code loaded!";
  LOG(INFO) << "CodeObject:\nid: " << code_object.id
            << "\nversion_string: " << code_object.version_string << "\njs:\n"
            << code_object.js;

  absl::Notification load_finished;
  LOG(INFO) << "Calling LoadCodeObj...";
  CHECK(
      roma_service
          ->LoadCodeObj(std::make_unique<CodeObject>(code_object),
                        [&load_finished](absl::StatusOr<ResponseObject> resp) {
                          if (resp.ok()) {
                            LOG(INFO) << "LoadCodeObj successful!";
                          } else {
                            std::cerr << "> load unsucessful with status: "
                                      << resp.status() << std::endl;
                          }
                          load_finished.Notify();
                        })
          .ok());
  load_finished.WaitForNotification();
}

void Execute(RomaService<>* roma_service,
             absl::Span<const std::string> tokens) {
  auto uuid = google::scp::core::common::Uuid::GenerateUuid();
  std::string uuid_str = google::scp::core::common::ToString(uuid);
  std::vector<std::string> input;
  std::transform(tokens.begin() + 2, tokens.end(), std::back_inserter(input),
                 [](std::string s) { return absl::StrCat("\"", s, "\""); });
  InvocationStrRequest<> execution_object = {.id = uuid_str,
                                             .version_string = tokens[0],
                                             .handler_name = tokens[1],
                                             .input = input};
  LOG(INFO) << "ExecutionObject:\nid: " << execution_object.id
            << "\nversion_string: " << execution_object.version_string
            << "\nhandler_name: " << execution_object.handler_name
            << "\ninput: " << absl::StrJoin(input, " ");
  std::string result;
  absl::Notification execute_finished;
  LOG(INFO) << "Calling Execute...";
  CHECK(roma_service
            ->Execute(
                std::make_unique<InvocationStrRequest<>>(execution_object),
                [&result,
                 &execute_finished](absl::StatusOr<ResponseObject> resp) {
                  if (resp.ok()) {
                    LOG(INFO) << "Execute successful!";
                    result = std::move(resp->resp);
                    std::cout << "> " << result << std::endl;
                  } else {
                    std::cerr << "> unsucessful with status: " << resp.status()
                              << std::endl;
                  }
                  execute_finished.Notify();
                })
            .ok());
  execute_finished.WaitForNotificationWithTimeout(kRequestTimeout);
}

// The read-eval-execute loop of the shell.
void RunShell() {
  RomaService<>::Config config;
  LOG(INFO) << "Roma config set to " << absl::GetFlag(FLAGS_num_workers)
            << " workers.";
  config.number_of_workers = absl::GetFlag(FLAGS_num_workers);

  LOG(INFO) << "Initializing RomaService...";
  auto roma_service = std::make_unique<RomaService<>>(std::move(config));
  CHECK(roma_service->Init().ok());
  LOG(INFO) << "RomaService Initialization successful.";

  std::cout << kCommandsMessage << std::endl;
  while (true) {
    std::cerr << "> ";
    std::string line;
    if (!std::getline(std::cin, line)) {
      break;
    }

    std::vector<std::string> tokens = absl::StrSplit(line, " ");

    if (tokens[0] == "exit") {
      (void)roma_service->Stop();
      break;
    } else if (tokens[0] == "load" && tokens.size() > 1) {
      Load(roma_service.get(), tokens[1], tokens.size() > 2 ? tokens[2] : "");
    } else if (tokens[0] == "execute" && tokens.size() > 2) {
      Execute(roma_service.get(),
              absl::Span(tokens.data() + 1, tokens.size() - 1));
    } else if (tokens[0] == "help") {
      std::cout << kCommandsMessage << std::endl;
    } else {
      std::cout << "Warning: unknown command " << tokens[0] << "." << std::endl;
      std::cout << "Try help for options." << std::endl;
    }
  }
}

int main(int argc, char* argv[]) {
  // Initialize ABSL.
  absl::InitializeLog();
  absl::SetProgramUsageMessage(
      "Opens a shell to allow for basic usage of the RomaService client to "
      "load and execute UDFs.");
  absl::ParseCommandLine(argc, argv);
  absl::SetStderrThreshold(absl::GetFlag(FLAGS_verbose)
                               ? absl::LogSeverity::kInfo
                               : absl::LogSeverity::kWarning);
  RunShell();

  return 0;
}