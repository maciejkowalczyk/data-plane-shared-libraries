/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UTIL_PROTOUTIL
#define UTIL_PROTOUTIL

// Utility functions for manipulating google::protobuf::Duration and
// google::protobuf::Timestamp, including conversions back and forth from
// absl::Duration and absl::Time.

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/timestamp.pb.h"

namespace privacy_sandbox::server_common {

// Encodes an absl::Duration as a google::protobuf::Duration.
// encoding rules specified at
// https://github.com/protocolbuffers/protobuf/blob/main/google/protobuf/duration.proto
// Returns an error if the given absl::Duration is beyond the range allowed by
// the protobuf. Otherwise, truncates toward zero with nanosecond precision and
// returns the google::protobuf::Duration.
//
// Note: +/- absl::InfiniteDuration() cannot be encoded because they are not
// representable in the protobuf.
absl::StatusOr<google::protobuf::Duration> EncodeGoogleApiProto(
    absl::Duration d);

// Same as above but taking a proto pointer.
absl::Status EncodeGoogleApiProto(absl::Duration d,
                                  google::protobuf::Duration* proto);

// Decodes the given protobuf and returns an absl::Duration, or returns an error
// status if the argument is invalid according to
// https://github.com/protocolbuffers/protobuf/blob/main/google/protobuf/duration.proto
absl::StatusOr<absl::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto);

// Returns the max Duration proto representing approximately 10,000 years,
// as defined by:
// https://github.com/protocolbuffers/protobuf/blob/main/google/protobuf/duration.proto
inline google::protobuf::Duration MakeGoogleApiDurationProtoMax() {
  google::protobuf::Duration proto;
  proto.set_seconds(315576000000);
  proto.set_nanos(999999999);
  return proto;
}

// Returns the max absl::Duration that can be represented as
// google::protobuf::Duration. Same as
// DecodeGoogleApiProto(MakeGoogleApiDurationProtoMax()).ValueOrDie().
inline absl::Duration MakeGoogleApiDurationMax() {
  return absl::Seconds(315576000000) + absl::Nanoseconds(999999999);
}

// Returns the min Duration proto representing approximately -10,000 years,
// as defined by:
// https://github.com/protocolbuffers/protobuf/blob/main/google/protobuf/duration.proto
inline google::protobuf::Duration MakeGoogleApiDurationProtoMin() {
  google::protobuf::Duration proto;
  proto.set_seconds(-315576000000);
  proto.set_nanos(-999999999);
  return proto;
}

// Returns the min absl::Duration that can be represented as
// google::protobuf::Duration.
inline absl::Duration MakeGoogleApiDurationMin() {
  return absl::Seconds(-315576000000) + absl::Nanoseconds(-999999999);
}

// Encodes an absl::Time as a google::protobuf::Timestamp, following the
// encoding rules specified at http://google3/google/protobuf/timestamp.proto.
// Returns an error if the given absl::Time is beyond the range allowed by the
// protobuf. Otherwise, truncates toward infinite past with nanosecond precision
// and returns the google::protobuf::Timestamp.
//
// Note: absl::InfiniteFuture/absl::InfinitePast() cannot be encoded because
// they are not representable in the protobuf.
absl::StatusOr<google::protobuf::Timestamp> EncodeGoogleApiProto(absl::Time t);

// Same as above but taking a proto pointer.
absl::Status EncodeGoogleApiProto(absl::Time t,
                                  google::protobuf::Timestamp* proto);

// Decodes the given protobuf and returns an absl::Time, or returns an error
// status if the argument is invalid according to
// http://google3/google/protobuf/timestamp.proto.
absl::StatusOr<absl::Time> DecodeGoogleApiProto(
    const google::protobuf::Timestamp& proto);

// Returns the max Timestamp proto representing 9999-12-31T23:59:59.999999999Z,
// as defined by: http://google3/google/protobuf/timestamp.proto
inline google::protobuf::Timestamp MakeGoogleApiTimestampProtoMax() {
  google::protobuf::Timestamp proto;
  proto.set_seconds(253402300799);
  proto.set_nanos(999999999);
  return proto;
}

// Returns the max absl::Time that can be represented as
// google::protobuf::Timestamp. Same as
// DecodeGoogleApiProto(MakeGoogleApiTimestampProtoMax()).ValueOrDie().
inline absl::Time MakeGoogleApiTimeMax() {
  return absl::UnixEpoch() + absl::Seconds(253402300799) +
         absl::Nanoseconds(999999999);
}

// Returns the min Timestamp proto representing 0001-01-01T00:00:00Z,
// as defined by: http://google3/google/protobuf/timestamp.proto
inline google::protobuf::Timestamp MakeGoogleApiTimestampProtoMin() {
  google::protobuf::Timestamp proto;
  proto.set_seconds(-62135596800);
  proto.set_nanos(0);
  return proto;
}

// Returns the min absl::Time that can be represented as
// google::protobuf::Timestamp.
inline absl::Time MakeGoogleApiTimeMin() {
  return absl::UnixEpoch() + absl::Seconds(-62135596800);
}

template <typename T>
T ParseTextOrDie(std::string_view text) {
  T message;
  CHECK(google::protobuf::TextFormat::ParseFromString(text.data(), &message));
  return message;
}

}  // namespace privacy_sandbox::server_common

#endif  // UTIL_PROTOUTIL
