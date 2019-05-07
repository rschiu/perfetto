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
 * perfetto/config/ftrace/ftrace_config.proto
 * by
 * ../../tools/proto_to_cpp/proto_to_cpp.cc.
 * If you need to make changes here, change the .proto file and then run
 * ./tools/gen_tracing_cpp_headers_from_protos
 */

#include "perfetto/tracing/core/ftrace_config.h"

#include "perfetto/config/ftrace/ftrace_config.pb.h"

namespace perfetto {

FtraceConfig::FtraceConfig() = default;
FtraceConfig::~FtraceConfig() = default;
FtraceConfig::FtraceConfig(const FtraceConfig&) = default;
FtraceConfig& FtraceConfig::operator=(const FtraceConfig&) = default;
FtraceConfig::FtraceConfig(FtraceConfig&&) noexcept = default;
FtraceConfig& FtraceConfig::operator=(FtraceConfig&&) = default;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
bool FtraceConfig::operator==(const FtraceConfig& other) const {
  return (ftrace_events_ == other.ftrace_events_) &&
         (atrace_categories_ == other.atrace_categories_) &&
         (atrace_apps_ == other.atrace_apps_) &&
         (ftrace_event_options_ == other.ftrace_event_options_) &&
         (buffer_size_kb_ == other.buffer_size_kb_) &&
         (drain_period_ms_ == other.drain_period_ms_);
}
#pragma GCC diagnostic pop

void FtraceConfig::FromProto(const perfetto::protos::FtraceConfig& proto) {
  ftrace_events_.clear();
  for (const auto& field : proto.ftrace_events()) {
    ftrace_events_.emplace_back();
    static_assert(
        sizeof(ftrace_events_.back()) == sizeof(proto.ftrace_events(0)),
        "size mismatch");
    ftrace_events_.back() =
        static_cast<decltype(ftrace_events_)::value_type>(field);
  }

  atrace_categories_.clear();
  for (const auto& field : proto.atrace_categories()) {
    atrace_categories_.emplace_back();
    static_assert(
        sizeof(atrace_categories_.back()) == sizeof(proto.atrace_categories(0)),
        "size mismatch");
    atrace_categories_.back() =
        static_cast<decltype(atrace_categories_)::value_type>(field);
  }

  atrace_apps_.clear();
  for (const auto& field : proto.atrace_apps()) {
    atrace_apps_.emplace_back();
    static_assert(sizeof(atrace_apps_.back()) == sizeof(proto.atrace_apps(0)),
                  "size mismatch");
    atrace_apps_.back() =
        static_cast<decltype(atrace_apps_)::value_type>(field);
  }

  ftrace_event_options_.clear();
  for (const auto& field : proto.ftrace_event_options()) {
    ftrace_event_options_.emplace_back();
    ftrace_event_options_.back().FromProto(field);
  }

  static_assert(sizeof(buffer_size_kb_) == sizeof(proto.buffer_size_kb()),
                "size mismatch");
  buffer_size_kb_ =
      static_cast<decltype(buffer_size_kb_)>(proto.buffer_size_kb());

  static_assert(sizeof(drain_period_ms_) == sizeof(proto.drain_period_ms()),
                "size mismatch");
  drain_period_ms_ =
      static_cast<decltype(drain_period_ms_)>(proto.drain_period_ms());
  unknown_fields_ = proto.unknown_fields();
}

void FtraceConfig::ToProto(perfetto::protos::FtraceConfig* proto) const {
  proto->Clear();

  for (const auto& it : ftrace_events_) {
    proto->add_ftrace_events(
        static_cast<decltype(proto->ftrace_events(0))>(it));
    static_assert(sizeof(it) == sizeof(proto->ftrace_events(0)),
                  "size mismatch");
  }

  for (const auto& it : atrace_categories_) {
    proto->add_atrace_categories(
        static_cast<decltype(proto->atrace_categories(0))>(it));
    static_assert(sizeof(it) == sizeof(proto->atrace_categories(0)),
                  "size mismatch");
  }

  for (const auto& it : atrace_apps_) {
    proto->add_atrace_apps(static_cast<decltype(proto->atrace_apps(0))>(it));
    static_assert(sizeof(it) == sizeof(proto->atrace_apps(0)), "size mismatch");
  }

  for (const auto& it : ftrace_event_options_) {
    auto* entry = proto->add_ftrace_event_options();
    it.ToProto(entry);
  }

  static_assert(sizeof(buffer_size_kb_) == sizeof(proto->buffer_size_kb()),
                "size mismatch");
  proto->set_buffer_size_kb(
      static_cast<decltype(proto->buffer_size_kb())>(buffer_size_kb_));

  static_assert(sizeof(drain_period_ms_) == sizeof(proto->drain_period_ms()),
                "size mismatch");
  proto->set_drain_period_ms(
      static_cast<decltype(proto->drain_period_ms())>(drain_period_ms_));
  *(proto->mutable_unknown_fields()) = unknown_fields_;
}

FtraceConfig::FtraceEventOption::FtraceEventOption() = default;
FtraceConfig::FtraceEventOption::~FtraceEventOption() = default;
FtraceConfig::FtraceEventOption::FtraceEventOption(
    const FtraceConfig::FtraceEventOption&) = default;
FtraceConfig::FtraceEventOption& FtraceConfig::FtraceEventOption::operator=(
    const FtraceConfig::FtraceEventOption&) = default;
FtraceConfig::FtraceEventOption::FtraceEventOption(
    FtraceConfig::FtraceEventOption&&) noexcept = default;
FtraceConfig::FtraceEventOption& FtraceConfig::FtraceEventOption::operator=(
    FtraceConfig::FtraceEventOption&&) = default;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
bool FtraceConfig::FtraceEventOption::operator==(
    const FtraceConfig::FtraceEventOption& other) const {
  return (event_id_ == other.event_id_) &&
         (enabled_fields_ == other.enabled_fields_);
}
#pragma GCC diagnostic pop

void FtraceConfig::FtraceEventOption::FromProto(
    const perfetto::protos::FtraceConfig_FtraceEventOption& proto) {
  static_assert(sizeof(event_id_) == sizeof(proto.event_id()), "size mismatch");
  event_id_ = static_cast<decltype(event_id_)>(proto.event_id());

  enabled_fields_.clear();
  for (const auto& field : proto.enabled_fields()) {
    enabled_fields_.emplace_back();
    static_assert(
        sizeof(enabled_fields_.back()) == sizeof(proto.enabled_fields(0)),
        "size mismatch");
    enabled_fields_.back() =
        static_cast<decltype(enabled_fields_)::value_type>(field);
  }
  unknown_fields_ = proto.unknown_fields();
}

void FtraceConfig::FtraceEventOption::ToProto(
    perfetto::protos::FtraceConfig_FtraceEventOption* proto) const {
  proto->Clear();

  static_assert(sizeof(event_id_) == sizeof(proto->event_id()),
                "size mismatch");
  proto->set_event_id(static_cast<decltype(proto->event_id())>(event_id_));

  for (const auto& it : enabled_fields_) {
    proto->add_enabled_fields(
        static_cast<decltype(proto->enabled_fields(0))>(it));
    static_assert(sizeof(it) == sizeof(proto->enabled_fields(0)),
                  "size mismatch");
  }
  *(proto->mutable_unknown_fields()) = unknown_fields_;
}

}  // namespace perfetto
