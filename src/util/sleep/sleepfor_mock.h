// Copyright 2024 Google LLC
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

#ifndef COMPONENTS_UTIL_SLEEPFOR_MOCK_H_
#define COMPONENTS_UTIL_SLEEPFOR_MOCK_H_

#include "gmock/gmock.h"
#include "src/util/sleep/sleepfor.h"

namespace privacy_sandbox::server_common {
class MockSleepFor : public SleepFor {
 public:
  MOCK_METHOD(bool, Duration, (absl::Duration), (const, override));
  MOCK_METHOD(absl::Status, Stop, (), (override));
};

class MockUnstoppableSleepFor : public UnstoppableSleepFor {
 public:
  MOCK_METHOD(bool, Duration, (absl::Duration), (const, override));
  MOCK_METHOD(absl::Status, Stop, (), (override));
};

}  // namespace privacy_sandbox::server_common

#endif  // COMPONENTS_UTIL_SLEEPFOR_MOCK_H_
