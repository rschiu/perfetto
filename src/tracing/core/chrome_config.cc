/*
 * Copyright (C) 2017 The Android Open Source Project
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

/*******************************************************************************
 * AUTOGENERATED - DO NOT EDIT
 *******************************************************************************
 * This file has been generated from the protobuf message
 * perfetto/config/chrome/chrome_config.proto
 * by
 * ../../tools/proto_to_cpp/proto_to_cpp.cc.
 * If you need to make changes here, change the .proto file and then run
 * ./tools/gen_tracing_cpp_headers_from_protos
 */

#include "perfetto/tracing/core/chrome_config.h"

#include "perfetto/config/chrome/chrome_config.pb.h"

namespace perfetto {

ChromeConfig::ChromeConfig() = default;
ChromeConfig::~ChromeConfig() = default;
ChromeConfig::ChromeConfig(const ChromeConfig&) = default;
ChromeConfig& ChromeConfig::operator=(const ChromeConfig&) = default;
ChromeConfig::ChromeConfig(ChromeConfig&&) noexcept = default;
ChromeConfig& ChromeConfig::operator=(ChromeConfig&&) = default;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
bool ChromeConfig::operator==(const ChromeConfig& other) const {
  return (trace_config_ == other.trace_config_) &&
         output_filtering_enabled_ == other.output_filtering_enabled_;
}
#pragma GCC diagnostic pop

void ChromeConfig::FromProto(const perfetto::protos::ChromeConfig& proto) {
  static_assert(sizeof(trace_config_) == sizeof(proto.trace_config()),
                "size mismatch");
  trace_config_ = static_cast<decltype(trace_config_)>(proto.trace_config());
  output_filtering_enabled_ = proto.output_filtering_enabled();
  unknown_fields_ = proto.unknown_fields();
}

void ChromeConfig::ToProto(perfetto::protos::ChromeConfig* proto) const {
  proto->Clear();

  static_assert(sizeof(trace_config_) == sizeof(proto->trace_config()),
                "size mismatch");
  proto->set_trace_config(
      static_cast<decltype(proto->trace_config())>(trace_config_));
  proto->set_output_filtering_enabled(output_filtering_enabled_);
  *(proto->mutable_unknown_fields()) = unknown_fields_;
}

}  // namespace perfetto
