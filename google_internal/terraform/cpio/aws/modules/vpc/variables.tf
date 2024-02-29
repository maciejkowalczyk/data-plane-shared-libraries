/**
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

variable "region" {
  description = "Cloud region."
  type        = string
}

variable "service" {
  description = "Assigned name of the CPIO Validator."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "instance_port" {
  description = "Port on which the enclave listens for TCP connections."
  default     = 51052
  type        = number
}

variable "cidr_blocks" {
  description = "IPs allowed to send traffic in the VPC."
  default     = "10.0.0.0/16"
  type        = string
}