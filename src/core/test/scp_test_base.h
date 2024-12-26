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

#ifndef CORE_TEST_SCP_TEST_BASE_H_
#define CORE_TEST_SCP_TEST_BASE_H_

#include <gtest/gtest.h>

namespace google::scp::core::test {
/**
 * @brief Base class for SCP tests
 */
class ScpTestBase : public ::testing::Test {
 public:
  /**
   * @brief Runs one time before the first test in the test suite
   */
  static void SetUpTestSuite() {
#if BUILD_FOR_DEBUGGER
    // Trap to force debugger entry breakpoint when debugging a test
    raise(SIGTRAP);
#endif
  }
};
}  // namespace google::scp::core::test

#endif  // CORE_TEST_SCP_TEST_BASE_H_
