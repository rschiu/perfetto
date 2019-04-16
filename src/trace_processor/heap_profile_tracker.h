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

#ifndef SRC_TRACE_PROCESSOR_HEAP_PROFILE_TRACKER_H_
#define SRC_TRACE_PROCESSOR_HEAP_PROFILE_TRACKER_H_

#include <deque>

#include "perfetto/trace/profiling/profile_packet.pbzero.h"
#include "src/trace_processor/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

struct HeapProfile {
  // Callsites table.
  std::deque<int64_t> callsite_id;
  std::deque<int64_t> callsite_frame_depth;
  std::deque<int64_t> callsite_parent_callsite_id;
  std::deque<int64_t> callsite_frame_id;

  // Frame table.
  std::deque<StringId> function_name;
  std::deque<int64_t> function_mapping;
  std::deque<int64_t> function_rel_pc;

  std::deque<StringId> mapping_build_id;
  std::deque<int64_t> mapping_offset;
  std::deque<int64_t> mapping_start;
  std::deque<int64_t> mapping_end;
  std::deque<int64_t> mapping_load_bias;
  std::deque<StringId> mapping_name;

  std::deque<int64_t> alloc_ts;
  std::deque<int64_t> alloc_pid;
  std::deque<int64_t> alloc_callsite_id;
  std::deque<int64_t> alloc_count;
  std::deque<int64_t> alloc_size;
};

class HeapProfileTracker {
 public:
  // Not the same as ProfilePacket.index. This gets only gets incremented when
  // encountering a ProfilePacket that is not confinuted.
  // This namespaces all other Source*Ids.
  using ProfileIndex = uint64_t;

  using SourceStringId = uint64_t;

  struct SourceMapping {
    SourceStringId build_id = 0;
    int64_t offset = 0;
    int64_t start = 0;
    int64_t end = 0;
    int64_t load_bias = 0;
    SourceStringId name = 0;
  };
  using SourceMappingId = uint64_t;

  struct SourceFrame {
    SourceStringId name_id = 0;
    SourceMappingId mapping_id = 0;
    uint64_t rel_pc = 0;
  };
  using SourceFrameId = uint64_t;

  using SourceCallstack = std::vector<SourceFrameId>;
  using SourceCallstackId = uint64_t;

  struct SourceAllocation {
    uint64_t pid = 0;
    uint64_t timestamp = 0;
    SourceCallstackId callstack_id = 0;
    uint64_t self_allocated = 0;
    uint64_t self_freed = 0;
    uint64_t alloc_count = 0;
    uint64_t free_count = 0;
  };

  HeapProfileTracker();

  void AddString(ProfileIndex, SourceStringId, StringId);
  void AddMapping(ProfileIndex, SourceMappingId, const SourceMapping&);
  void AddFrame(ProfileIndex, SourceFrameId, const SourceFrame&);
  void AddCallstack(ProfileIndex, SourceCallstackId, const SourceCallstack&);
  void AddAllocation(ProfileIndex, const SourceAllocation&);

  ~HeapProfileTracker();

 private:
  int64_t FindOrInsertSourceMapping(ProfileIndex, const SourceMapping&);
  int64_t FindOrInsertSourceFrame(ProfileIndex, const SourceFrame&);
  int64_t FindOrInsertSourceCallstack(ProfileIndex, const SourceCallstack&);

  std::map<std::pair<ProfileIndex, SourceStringId>, StringId> string_map_;

  std::map<std::pair<ProfileIndex, SourceFrameId>, int64_t> frames_;
  std::map<std::pair<ProfileIndex, std::vector<SourceFrameId>>, int64_t>
      callstacks_from_frames_;
  std::map<std::pair<ProfileIndex, SourceCallstackId>, int64_t> callstacks_;

  HeapProfile cur_heap_profile_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_HEAP_PROFILE_TRACKER_H_
