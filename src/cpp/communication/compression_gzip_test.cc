// Copyright 2023 Google LLC
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

#include "src/cpp/communication/compression_gzip.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "quiche/common/quiche_data_writer.h"
#include "src/cpp/communication/uncompressed.h"

namespace privacy_sandbox::server_common {
namespace {

using ::boost::iostreams::copy;
using ::boost::iostreams::filtering_istreambuf;
using ::boost::iostreams::gzip_compressor;
using ::boost::iostreams::gzip_decompressor;
using ::boost::iostreams::gzip_params;
using ::boost::iostreams::gzip::best_compression;

std::string BoostCompress(absl::string_view decompressed_string) {
  std::istringstream origin(decompressed_string.data());

  filtering_istreambuf buffer;
  buffer.push(gzip_compressor(gzip_params(best_compression)));
  buffer.push(origin);

  std::ostringstream compressed_string;
  copy(buffer, compressed_string);
  return compressed_string.str();
}

std::string BoostDecompress(const std::string& compressed_string) {
  std::istringstream compressed(compressed_string);

  filtering_istreambuf buffer;
  buffer.push(gzip_decompressor());
  buffer.push(compressed);

  std::ostringstream decompressed_string;
  copy(buffer, decompressed_string);
  return decompressed_string.str();
}

TEST(GzipCompressionTests, CompressDecompress_EndToEnd) {
  std::string payload = "hello";

  GzipCompressionGroupConcatenator concatenator;
  concatenator.AddCompressionGroup(payload);
  absl::StatusOr<std::string> compressed = concatenator.Build();

  GzipCompressionBlobReader blob_reader(*compressed);
  absl::StatusOr<std::string> compression_group =
      blob_reader.ExtractOneCompressionGroup();
  ASSERT_TRUE(compression_group.ok());
  ASSERT_EQ(payload, compression_group.value());
}

TEST(GzipCompressionTests, CompressWithBoost) {
  // Verify that a gzip compressed string from another library can also be
  // decompressed successfully using zlib's decompressor.
  std::string payload = "hello";
  std::string compressed = BoostCompress(payload);

  int partition_size = sizeof(uint32_t) + compressed.size();
  std::string partition(partition_size, '0');

  quiche::QuicheDataWriter data_writer(partition_size, partition.data());
  data_writer.WriteUInt32(compressed.size());
  data_writer.WriteStringPiece(compressed);

  GzipCompressionBlobReader blob_reader(partition);
  absl::StatusOr<std::string> compression_group =
      blob_reader.ExtractOneCompressionGroup();
  ASSERT_TRUE(compression_group.ok());
  ASSERT_EQ(payload, compression_group.value());
}

TEST(GzipCompressionTests, DecompressWithBoost) {
  // Verify a string compressed using zlib can be decompressed by another
  // library as well (Boost).
  std::string payload = "hello";

  GzipCompressionGroupConcatenator concatenator;
  concatenator.AddCompressionGroup(payload);
  absl::StatusOr<std::string> compressed = concatenator.Build();

  std::string boost_decompress = BoostDecompress(compressed->substr(4));
  ASSERT_EQ(payload, boost_decompress);
}

}  // namespace
}  // namespace privacy_sandbox::server_common
