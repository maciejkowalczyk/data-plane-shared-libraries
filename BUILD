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

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")

string_flag(
    name = "platform",
    build_setting_default = "aws",
    values = [
        "aws",
        "gcp",
        "local",
    ],
)

config_setting(
    name = "aws_platform",
    flag_values = {
        ":platform": "aws",
    },
    visibility = ["//visibility:private"],
)

config_setting(
    name = "gcp_platform",
    flag_values = {
        ":platform": "gcp",
    },
    visibility = ["//visibility:private"],
)

config_setting(
    name = "local_platform",
    flag_values = {
        ":platform": "local",
    },
    visibility = ["//visibility:private"],
)

string_flag(
    name = "instance",
    build_setting_default = "aws",
    values = [
        "aws",
        "gcp",
        "local",
    ],
)

config_setting(
    name = "aws_instance",
    flag_values = {
        ":instance": "aws",
    },
    visibility = ["//visibility:public"],
)

config_setting(
    name = "gcp_instance",
    flag_values = {
        ":instance": "gcp",
    },
    visibility = ["//visibility:public"],
)

config_setting(
    name = "local_instance",
    flag_values = {
        ":instance": "local",
    },
    visibility = ["//visibility:public"],
)
