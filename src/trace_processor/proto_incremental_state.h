/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_PROTO_INCREMENTAL_STATE_H_
#define SRC_TRACE_PROCESSOR_PROTO_INCREMENTAL_STATE_H_

#include <stdint.h>

#include <map>
#include <unordered_map>

#include "perfetto/protozero/proto_decoder.h"
#include "src/trace_processor/trace_blob_view.h"

#include "perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "perfetto/trace/track_event/task_execution.pbzero.h"
#include "perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
namespace trace_processor {

class ProtoIncrementalState {
 public:
  template <typename MessageType>
  struct InternedDataView {
    typename MessageType::Decoder GetDecoder() {
      return MessageType::Decoder(message.data(), message.length());
    }

    TraceBlobView message;
  };

  template <typename MessageType>
  using InternedDataMap =
      std::unordered_map<uint32_t, InternedDataView<MessageType>>;

  class PacketSequenceState {
   public:
    void SetTrackEventTimeNs(int64_t timestamp) {
      track_event_timestamp_ns_ = timestamp;
    }

    int64_t IncrementAndGetTrackEventTimeNs(int64_t delta_ns) {
      track_event_timestamp_ns_ += delta_ns;
      return track_event_timestamp_ns_;
    }

    void SetTrackEventThreadTimeNs(int64_t timestamp) {
      track_event_timestamp_ns_ = timestamp;
    }

    int64_t IncrementAndGetTrackEventThreadTimeNs(int64_t delta_ns) {
      track_event_thread_timestamp_ns_ += delta_ns;
      return track_event_thread_timestamp_ns_;
    }

    void SetPacketLoss() {
      packet_loss_ = true;
      thread_descriptor_seen_ = false;
    }

    void SetIncrementalStateCleared() { packet_loss_ = false; }

    void SetThreadDescriptor(int32_t pid, int32_t tid) {
      thread_descriptor_seen_ = true;
      pid_ = pid;
      tid_ = tid;
    }

    bool IsIncrementalStateValid() { return !packet_loss_; }

    bool IsTrackEventStateValid() {
      return IsIncrementalStateValid() && thread_descriptor_seen_;
    }

    int32_t GetPid() const { return pid_; }
    int32_t GetTid() const { return tid_; }

    template <typename MessageType>
    InternedDataMap<MessageType>* GetInternedDataMap();

    template <>
    InternedDataMap<protos::pbzero::EventCategory>* GetInternedDataMap() {
      return &event_categories_;
    }

    template <>
    InternedDataMap<protos::pbzero::LegacyEventName>* GetInternedDataMap() {
      return &legacy_event_names_;
    }

    template <>
    InternedDataMap<protos::pbzero::DebugAnnotationName>* GetInternedDataMap() {
      return &debug_annotation_names_;
    }

    template <>
    InternedDataMap<protos::pbzero::SourceLocation>* GetInternedDataMap() {
      return &source_locations_;
    }

   private:
    // If true, incremental state on the sequence is considered invalid until we
    // see the next packet with incremental_state_cleared. We assume that we
    // missed some packets at the beginning of the trace.
    bool packet_loss_ = true;

    // We can only consider TrackEvent delta timestamps to be correct after we
    // have observed a thread descriptor (since the last packet loss).
    bool thread_descriptor_seen_ = false;

    // Process/thread ID of the packet sequence. Used as default values for
    // TrackEvents that don't specify a pid/tid override. Only valid while
    // |seen_thread_descriptor_| is true.
    int32_t pid_ = 0;
    int32_t tid_ = 0;

    // Current wall/thread timestamps used as reference for the next TrackEvent
    // delta timestamp.
    int64_t track_event_timestamp_ns_ = 0;
    int64_t track_event_thread_timestamp_ns_ = 0;

    InternedDataMap<protos::pbzero::EventCategory> event_categories_;
    InternedDataMap<protos::pbzero::LegacyEventName> legacy_event_names_;
    InternedDataMap<protos::pbzero::DebugAnnotationName>
        debug_annotation_names_;
    InternedDataMap<protos::pbzero::SourceLocation> source_locations_;
  };

  PacketSequenceState* GetStateForPacketSequence(uint32_t sequence_id) {
    return &packet_sequence_states_[sequence_id];
  }

 private:
  std::map<uint32_t, PacketSequenceState> packet_sequence_states_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PROTO_INCREMENTAL_STATE_H_
