# Copyright 2022 Google LLC
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Loads com_github_grpc_grpc.
# Defines @com_github_grpc_grpc before @com_github_googleapis_google_cloud_cpp
# to override the dependenices in @com_github_googleapis_google_cloud_cpp.

def grpc():
    maybe(
        http_archive,
        name = "com_github_grpc_grpc",
        patch_args = ["-p1"],
        patches = [
            Label("//build_defs/shared:grpc.patch"),
        ],
        sha256 = "a8922a05b196ef759a2745ffd965413dd177949fb373e648569d5458fdc515d4",
        strip_prefix = "grpc-1.64.0",
        urls = [
            "https://github.com/grpc/grpc/archive/refs/tags/v1.64.0.zip",
        ],
    )
