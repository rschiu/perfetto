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

#include "src/trace_processor/heap_profile_tracker.h"

#include "src/trace_processor/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

HeapProfileTracker::HeapProfileTracker() = default;
HeapProfileTracker::~HeapProfileTracker() = default;

void HeapProfileTracker::AddString(ProfileIndex pidx,
                                   SourceStringId id,
                                   StringId str) {
  string_map_.emplace(std::make_pair(pidx, id), str);
}

void HeapProfileTracker::AddMapping(ProfileIndex pidx,
                                    SourceMappingId id,
                                    const SourceMapping& mapping) {
  base::ignore_result(pidx);
  base::ignore_result(id);
  base::ignore_result(mapping);
}

void HeapProfileTracker::AddFrame(ProfileIndex pidx,
                                  SourceFrameId id,
                                  const SourceFrame& frame) {
  int64_t cur_row =
      static_cast<int64_t>(cur_heap_profile_.function_name.size());
  auto it = string_map_.find({pidx, frame.name_id});
  if (it == string_map_.end()) {
    PERFETTO_DFATAL("Unknown string.");
    return;
  }
  const StringId& str_id = it->second;
  cur_heap_profile_.function_name.emplace_back(str_id);
  frames_.emplace(std::make_pair(pidx, id), cur_row);
}

void HeapProfileTracker::AddCallstack(ProfileIndex pidx,
                                      SourceCallstackId id,
                                      const SourceCallstack& frame_ids) {
  int64_t next_id = 1;
  if (!cur_heap_profile_.callsite_id.empty())
    next_id = cur_heap_profile_.callsite_id.back() + 1;

  int64_t parent_id = 0;
  for (size_t depth = 0; depth < frame_ids.size(); ++depth) {
    std::vector<uint64_t> frame_subset = frame_ids;
    frame_subset.resize(depth + 1);
    auto self_it = callstacks_from_frames_.find({pidx, frame_subset});
    if (self_it != callstacks_from_frames_.end()) {
      parent_id = self_it->second;
      continue;
    }

    uint64_t frame_id = frame_ids[depth];
    auto it = frames_.find({pidx, frame_id});
    if (it == frames_.end()) {
      PERFETTO_DFATAL("Unknown frames.");
      return;
    }
    int64_t frame_row = it->second;

    int64_t self_id = next_id++;
    cur_heap_profile_.callsite_id.emplace_back(self_id);
    cur_heap_profile_.callsite_frame_depth.emplace_back(depth);
    cur_heap_profile_.callsite_parent_callsite_id.emplace_back(parent_id);
    cur_heap_profile_.callsite_frame_id.emplace_back(frame_row);
    callstacks_.emplace(std::make_pair(pidx, id), self_id);
    parent_id = self_id;
  }
}

void HeapProfileTracker::AddAllocation(ProfileIndex pidx,
                                       const SourceAllocation& alloc) {
  auto it = callstacks_.find({pidx, alloc.callstack_id});
  if (it == callstacks_.end()) {
    PERFETTO_DFATAL("Unknown callstack");
    return;
  }

  cur_heap_profile_.alloc_ts.emplace_back(alloc.timestamp);
  cur_heap_profile_.alloc_pid.emplace_back(alloc.pid);
  cur_heap_profile_.alloc_callsite_id.emplace_back(it->second);
  cur_heap_profile_.alloc_count.emplace_back(alloc.alloc_count);
  cur_heap_profile_.alloc_size.emplace_back(alloc.self_allocated);

  cur_heap_profile_.alloc_ts.emplace_back(alloc.timestamp);
  cur_heap_profile_.alloc_pid.emplace_back(alloc.pid);
  cur_heap_profile_.alloc_callsite_id.emplace_back(it->second);
  cur_heap_profile_.alloc_count.emplace_back(-alloc.free_count);
  cur_heap_profile_.alloc_size.emplace_back(-alloc.self_freed);
}

}  // namespace trace_processor
}  // namespace perfetto
