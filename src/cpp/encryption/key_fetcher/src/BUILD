# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "public_key_fetcher",
    srcs = ["public_key_fetcher.cc"],
    hdrs = ["public_key_fetcher.h"],
    deps = [
        ":key_fetcher_utils",
        "//src/cpp/encryption/key_fetcher/interface:public_key_fetcher_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings:str_format",
        "@control_plane_shared//cc/public/cpio/interface/public_key_client",
    ],
)

cc_library(
    name = "private_key_fetcher",
    srcs = ["private_key_fetcher.cc"],
    hdrs = ["private_key_fetcher.h"],
    deps = [
        ":key_fetcher_utils",
        "//src/cpp/encryption/key_fetcher/interface:private_key_fetcher_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@control_plane_shared//cc/public/cpio/interface/crypto_client",
        "@control_plane_shared//cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
    ],
)

cc_library(
    name = "fake_key_fetcher_manager",
    srcs = ["fake_key_fetcher_manager.cc"],
    hdrs = ["fake_key_fetcher_manager.h"],
    deps = [
        "//src/cpp/encryption/key_fetcher/interface:key_fetcher_manager_interface",
        "//src/cpp/encryption/key_fetcher/interface:private_key_fetcher_interface",
        "//src/cpp/encryption/key_fetcher/interface:public_key_fetcher_interface",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "key_fetcher_manager",
    srcs = ["key_fetcher_manager.cc"],
    hdrs = ["key_fetcher_manager.h"],
    deps = [
        "//src/cpp/concurrent:executor",
        "//src/cpp/encryption/key_fetcher/interface:key_fetcher_manager_interface",
        "//src/cpp/encryption/key_fetcher/interface:private_key_fetcher_interface",
        "//src/cpp/encryption/key_fetcher/interface:public_key_fetcher_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "key_fetcher_utils",
    srcs = ["key_fetcher_utils.cc"],
    hdrs = ["key_fetcher_utils.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)
