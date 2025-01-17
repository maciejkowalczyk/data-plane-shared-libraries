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

#include <string>

#include "src/cpp/communication/compression.h"

namespace privacy_sandbox::server_common {

// Builds compression groups that are compressed by Brotli.
class BrotliCompressionGroupConcatenator : public CompressionGroupConcatenator {
 public:
  absl::StatusOr<std::string> Build() const override;
};

// Reads compression groups built with BrotliCompressionGroupConcatenator.
class BrotliCompressionBlobReader : public CompressedBlobReader {
 public:
  explicit BrotliCompressionBlobReader(std::string_view compressed)
      : CompressedBlobReader(compressed) {}

  absl::StatusOr<std::string> ExtractOneCompressionGroup() override;
};

}  // namespace privacy_sandbox::server_common
