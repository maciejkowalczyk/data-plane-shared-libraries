// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/roma/byob/tools/shell_evaluator.h"

#include <fstream>
#include <iostream>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"

namespace privacy_sandbox::server_common::byob {
ShellEvaluator::ShellEvaluator(
    std::string_view service_specific_message, std::vector<std::string> rpcs,
    absl::FunctionRef<absl::StatusOr<std::string>(std::string_view)> load_fn,
    absl::FunctionRef<absl::StatusOr<std::string>(
        std::string_view, std::string_view, std::istream&)>
        execute_fn)
    : service_specific_message_(service_specific_message),
      load_fn_(load_fn),
      execute_fn_(execute_fn) {
  rpc_to_token_.reserve(rpcs.size());
  for (auto& rpc : rpcs) {
    rpc_to_token_[std::move(rpc)] = std::nullopt;
  }
}

namespace {
constexpr std::string_view kHelpMessage = R"(Shell Commands:

help - Display all shell commands
Usage: help

commands - Execute commands from specified filename
    Note: Recursion is not permitted.
Usage: commands <commands_file>

exit - Exit the tool
Usage: exit
)";
}  // namespace

ShellEvaluator::NextStep ShellEvaluator::EvalAndPrint(std::string_view line,
                                                      bool disable_commands) {
  const std::vector<std::string_view> command =
      absl::StrSplit(line, ' ', absl::SkipWhitespace());
  if (command.empty()) {
    return NextStep::kContinue;
  }
  if (command.front() == "exit") {
    return NextStep::kExit;
  }
  if (command.front() == "help" || command.front() == "h" ||
      command.front() == "?") {
    std::cout << kHelpMessage << service_specific_message_;
  } else if (command.front() == "commands") {
    if (disable_commands) {
      std::cerr << "`commands` command is disabled\n";
      return NextStep::kError;
    }
    if (command.size() != 2) {
      std::cerr << "commands <commands_file>\n";
      return NextStep::kError;
    }
    std::ifstream ifs(std::string{command[1]});
    if (!ifs.is_open()) {
      std::cerr << "Failed to open '" << command[1] << "'\n";
      return NextStep::kError;
    }
    std::string line;
    while (std::getline(ifs, line)) {
      const NextStep loop_next_step =
          EvalAndPrint(line, /*disable_commands=*/true);
      switch (loop_next_step) {
        case NextStep::kExit:
        case NextStep::kError:
          return loop_next_step;
        case NextStep::kContinue:
          continue;
      }
    }
  } else if (command.front() == "load" || command.front() == "l") {
    if (command.size() != 3) {
      std::cerr << "load <rpc_command> <udf_file>\n";
      return NextStep::kError;
    }
    const auto it = rpc_to_token_.find(command[1]);
    if (it == rpc_to_token_.end()) {
      std::cerr << "Unrecognized rpc command '" << command[1] << "'\n";
      return NextStep::kError;
    }
    absl::StatusOr<std::string> code_token = load_fn_(command[2]);
    if (!code_token.ok()) {
      std::cerr << "load error: " << code_token.status() << "\n";
      return NextStep::kError;
    }
    std::cout << "code_token=" << *code_token << "\n";
    it->second = *std::move(code_token);
  } else {
    const auto it = rpc_to_token_.find(command.front());
    if (it == rpc_to_token_.end()) {
      std::cerr << "Unrecognized command '" << command.front() << "'\n";
      return NextStep::kError;
    }
    if (command.size() != 2 && command.size() != 3) {
      std::cerr << command.front() << " <request_file> [response_file]\n";
      return NextStep::kError;
    }
    if (!it->second.has_value()) {
      std::cerr << "No UDF loaded for '" << command.front() << "'\n";
      return NextStep::kError;
    }
    std::ifstream ifs(std::string{command[1]});
    if (!ifs.is_open()) {
      std::cerr << "Failed to open '" << command[1] << "'\n";
      return NextStep::kError;
    }
    const absl::StatusOr<std::string> serialized_response =
        execute_fn_(command.front(), *it->second, ifs);
    if (!serialized_response.ok()) {
      std::cerr << command.front() << " error: " << serialized_response.status()
                << "\n";
      return NextStep::kError;
    }
    if (command.size() == 3) {
      std::ofstream ofs;
      ofs.open(std::string{command[2]}, std::ios_base::app);
      ofs << *serialized_response;
    }
    std::cout << "{code_token=" << *it->second
              << ", response=" << *serialized_response << "}\n";
  }
  return NextStep::kContinue;
}
}  // namespace privacy_sandbox::server_common::byob