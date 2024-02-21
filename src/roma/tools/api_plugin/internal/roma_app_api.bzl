# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Helper rules and protoc plugin structs for the Roma Application API."""

load("@rules_proto_grpc//:defs.bzl", "ProtoPluginInfo", "proto_compile_attrs", "proto_compile_impl")

# the template_dir_name must be a valid string value for unix directories, the
# path does not need to exist -- the golang plugin and the plugin options both
# use this string value
template_dir_name = "roma-app-api-templates"

def _get_template_options(suffix, template_file):
    return "{tmpl_dir}/tmpl/{tmpl},{basename}{suffix}".format(
        tmpl_dir = template_dir_name,
        basename = "{basename}",
        suffix = suffix,
        tmpl = template_file,
    )

_cc_protobuf_plugins = [
    struct(
        name = "cpp_proto_plugin",
        exclusions = [
            "google/protobuf",
        ],
        outputs = [
            "{protopath}.pb.h",
            "{protopath}.pb.cc",
        ],
        protoc_plugin_name = "cpp",
        tool = "@com_google_protobuf//:protoc",
    ),
]

_cc_template_plugins = [
    struct(
        name = "roma_app_api_cc_plugin{}".format(i),
        option = _get_template_options(suffix, template_file),
        outputs = ["{basename}" + suffix],
        tool = "//src/roma/tools/api_plugin:app_api_plugin",
    )
    for i, (template_file, suffix) in enumerate([
        ("cpp_markdown.tmpl", "_cpp_client_api.md"),
    ])
]

app_api_cc_plugins = _cc_protobuf_plugins + _cc_template_plugins

app_api_cc_protoc = rule(
    implementation = proto_compile_impl,
    attrs = dict(
        proto_compile_attrs,
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [Label("//src/roma/tools/api_plugin:{}".format(plugin.name)) for plugin in app_api_cc_plugins],
            doc = "List of protoc plugins to apply",
        ),
    ),
    toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
)

def get_plugins():
    return app_api_cc_plugins