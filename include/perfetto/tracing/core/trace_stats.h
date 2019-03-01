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
 * perfetto/common/trace_stats.proto
 * by
 * ../../tools/proto_to_cpp/proto_to_cpp.cc.
 * If you need to make changes here, change the .proto file and then run
 * ./tools/gen_tracing_cpp_headers_from_protos
 */

#ifndef INCLUDE_PERFETTO_TRACING_CORE_TRACE_STATS_H_
#define INCLUDE_PERFETTO_TRACING_CORE_TRACE_STATS_H_

#include <stdint.h>
#include <string>
#include <type_traits>
#include <vector>

#include "perfetto/base/export.h"

// Forward declarations for protobuf types.
namespace perfetto {
namespace protos {
class TraceStats;
class TraceStats_BufferStats;
}  // namespace protos
}  // namespace perfetto

namespace perfetto {

class PERFETTO_EXPORT TraceStats {
 public:
  class PERFETTO_EXPORT BufferStats {
   public:
    BufferStats();
    ~BufferStats();
    BufferStats(BufferStats&&) noexcept;
    BufferStats& operator=(BufferStats&&);
    BufferStats(const BufferStats&);
    BufferStats& operator=(const BufferStats&);
    bool operator==(const BufferStats&) const;

    // Conversion methods from/to the corresponding protobuf types.
    void FromProto(const perfetto::protos::TraceStats_BufferStats&);
    void ToProto(perfetto::protos::TraceStats_BufferStats*) const;

    uint64_t buffer_size() const { return buffer_size_; }
    void set_buffer_size(uint64_t value) { buffer_size_ = value; }

    uint64_t bytes_written() const { return bytes_written_; }
    void set_bytes_written(uint64_t value) { bytes_written_ = value; }

    uint64_t bytes_overwritten() const { return bytes_overwritten_; }
    void set_bytes_overwritten(uint64_t value) { bytes_overwritten_ = value; }

    uint64_t bytes_read() const { return bytes_read_; }
    void set_bytes_read(uint64_t value) { bytes_read_ = value; }

    uint64_t padding_bytes_written() const { return padding_bytes_written_; }
    void set_padding_bytes_written(uint64_t value) {
      padding_bytes_written_ = value;
    }

    uint64_t padding_bytes_cleared() const { return padding_bytes_cleared_; }
    void set_padding_bytes_cleared(uint64_t value) {
      padding_bytes_cleared_ = value;
    }

    uint64_t chunks_written() const { return chunks_written_; }
    void set_chunks_written(uint64_t value) { chunks_written_ = value; }

    uint64_t chunks_rewritten() const { return chunks_rewritten_; }
    void set_chunks_rewritten(uint64_t value) { chunks_rewritten_ = value; }

    uint64_t chunks_overwritten() const { return chunks_overwritten_; }
    void set_chunks_overwritten(uint64_t value) { chunks_overwritten_ = value; }

    uint64_t chunks_discarded() const { return chunks_discarded_; }
    void set_chunks_discarded(uint64_t value) { chunks_discarded_ = value; }

    uint64_t chunks_read() const { return chunks_read_; }
    void set_chunks_read(uint64_t value) { chunks_read_ = value; }

    uint64_t chunks_committed_out_of_order() const {
      return chunks_committed_out_of_order_;
    }
    void set_chunks_committed_out_of_order(uint64_t value) {
      chunks_committed_out_of_order_ = value;
    }

    uint64_t write_wrap_count() const { return write_wrap_count_; }
    void set_write_wrap_count(uint64_t value) { write_wrap_count_ = value; }

    uint64_t patches_succeeded() const { return patches_succeeded_; }
    void set_patches_succeeded(uint64_t value) { patches_succeeded_ = value; }

    uint64_t patches_failed() const { return patches_failed_; }
    void set_patches_failed(uint64_t value) { patches_failed_ = value; }

    uint64_t readaheads_succeeded() const { return readaheads_succeeded_; }
    void set_readaheads_succeeded(uint64_t value) {
      readaheads_succeeded_ = value;
    }

    uint64_t readaheads_failed() const { return readaheads_failed_; }
    void set_readaheads_failed(uint64_t value) { readaheads_failed_ = value; }

    uint64_t abi_violations() const { return abi_violations_; }
    void set_abi_violations(uint64_t value) { abi_violations_ = value; }

   private:
    uint64_t buffer_size_ = {};
    uint64_t bytes_written_ = {};
    uint64_t bytes_overwritten_ = {};
    uint64_t bytes_read_ = {};
    uint64_t padding_bytes_written_ = {};
    uint64_t padding_bytes_cleared_ = {};
    uint64_t chunks_written_ = {};
    uint64_t chunks_rewritten_ = {};
    uint64_t chunks_overwritten_ = {};
    uint64_t chunks_discarded_ = {};
    uint64_t chunks_read_ = {};
    uint64_t chunks_committed_out_of_order_ = {};
    uint64_t write_wrap_count_ = {};
    uint64_t patches_succeeded_ = {};
    uint64_t patches_failed_ = {};
    uint64_t readaheads_succeeded_ = {};
    uint64_t readaheads_failed_ = {};
    uint64_t abi_violations_ = {};

    // Allows to preserve unknown protobuf fields for compatibility
    // with future versions of .proto files.
    std::string unknown_fields_;
  };

  TraceStats();
  ~TraceStats();
  TraceStats(TraceStats&&) noexcept;
  TraceStats& operator=(TraceStats&&);
  TraceStats(const TraceStats&);
  TraceStats& operator=(const TraceStats&);
  bool operator==(const TraceStats&) const;

  // Conversion methods from/to the corresponding protobuf types.
  void FromProto(const perfetto::protos::TraceStats&);
  void ToProto(perfetto::protos::TraceStats*) const;

  int buffer_stats_size() const {
    return static_cast<int>(buffer_stats_.size());
  }
  const std::vector<BufferStats>& buffer_stats() const { return buffer_stats_; }
  BufferStats* add_buffer_stats() {
    buffer_stats_.emplace_back();
    return &buffer_stats_.back();
  }

  uint32_t producers_connected() const { return producers_connected_; }
  void set_producers_connected(uint32_t value) { producers_connected_ = value; }

  uint64_t producers_seen() const { return producers_seen_; }
  void set_producers_seen(uint64_t value) { producers_seen_ = value; }

  uint32_t data_sources_registered() const { return data_sources_registered_; }
  void set_data_sources_registered(uint32_t value) {
    data_sources_registered_ = value;
  }

  uint64_t data_sources_seen() const { return data_sources_seen_; }
  void set_data_sources_seen(uint64_t value) { data_sources_seen_ = value; }

  uint32_t tracing_sessions() const { return tracing_sessions_; }
  void set_tracing_sessions(uint32_t value) { tracing_sessions_ = value; }

  uint32_t total_buffers() const { return total_buffers_; }
  void set_total_buffers(uint32_t value) { total_buffers_ = value; }

  uint64_t chunks_discarded() const { return chunks_discarded_; }
  void set_chunks_discarded(uint64_t value) { chunks_discarded_ = value; }

  uint64_t patches_discarded() const { return patches_discarded_; }
  void set_patches_discarded(uint64_t value) { patches_discarded_ = value; }

 private:
  std::vector<BufferStats> buffer_stats_;
  uint32_t producers_connected_ = {};
  uint64_t producers_seen_ = {};
  uint32_t data_sources_registered_ = {};
  uint64_t data_sources_seen_ = {};
  uint32_t tracing_sessions_ = {};
  uint32_t total_buffers_ = {};
  uint64_t chunks_discarded_ = {};
  uint64_t patches_discarded_ = {};

  // Allows to preserve unknown protobuf fields for compatibility
  // with future versions of .proto files.
  std::string unknown_fields_;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_CORE_TRACE_STATS_H_
