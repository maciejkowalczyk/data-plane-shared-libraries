# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Further initialization of shared control plane dependencies."""

load("@com_github_googleapis_google_cloud_cpp//bazel:google_cloud_cpp_deps.bzl", "google_cloud_cpp_deps")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@com_google_sandboxed_api//sandboxed_api/bazel:llvm_config.bzl", "llvm_disable_optional_support_deps")
load("@com_google_sandboxed_api//sandboxed_api/bazel:sapi_deps.bzl", "sapi_deps")
load("@io_bazel_rules_docker//repositories:repositories.bzl", container_repositories = "repositories")
load("@io_opentelemetry_cpp//bazel:repository.bzl", "opentelemetry_cpp_deps")
load("@rules_buf//buf:repositories.bzl", "rules_buf_dependencies", "rules_buf_toolchains")
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
load("@tink_cc//:tink_cc_deps.bzl", "tink_cc_deps")
load("@v8_python_deps//:requirements.bzl", install_v8_python_deps = "install_deps")

def deps3():
    protobuf_deps()
    google_cloud_cpp_deps()
    llvm_disable_optional_support_deps()
    sapi_deps()

    # This sets up some common toolchains for building targets. For more details, please see
    # https://bazelbuild.github.io/rules_foreign_cc/0.9.0/flatten.html#rules_foreign_cc_dependencies
    rules_foreign_cc_dependencies()
    install_v8_python_deps()

    rules_buf_dependencies()
    rules_buf_toolchains(version = "v1.7.0")
    tink_cc_deps()
    container_repositories()
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        grpc = True,
    )
    opentelemetry_cpp_deps()
