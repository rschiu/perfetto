/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/json_trace_parser.h"

#include <inttypes.h>
#include <json/reader.h>
#include <json/value.h>
#include <sajson.h>

#include <limits>
#include <string>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/utils.h"
#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/slice_tracker.h"
#include "src/trace_processor/trace_processor_context.h"

#if !PERFETTO_BUILDFLAG(PERFETTO_STANDALONE_BUILD)
#error The JSON trace parser is supported only in the standalone build for now.
#endif

namespace perfetto {
namespace trace_processor {
namespace {

enum ReadDictRes { kFoundDict, kNeedsMoreData, kEndOfTrace };

// Parses at most one JSON dictionary and returns a pointer to the end of it,
// or nullptr if no dict could be detected.
// This is to avoid decoding the full trace in memory and reduce heap traffic.
// E.g.  input:  { a:1 b:{ c:2, d:{ e:3 } } } , { a:4, ... },
//       output: [   only this is parsed    ] ^return value points here.
ReadDictRes ReadOneJsonDict(const char* start,
                            const char* end,
                            const char** begin,
                            const char** next) {
  int braces = 0;
  const char* dict_begin = nullptr;
  for (const char* s = start; s < end; s++) {
    if (isspace(*s) || *s == ',')
      continue;
    if (*s == '{') {
      if (braces == 0)
        dict_begin = s;
      braces++;
      continue;
    }
    if (*s == '}') {
      if (braces <= 0)
        return kEndOfTrace;
      if (--braces > 0)
        continue;

      *begin = dict_begin;

      // Json::Reader reader;
      // if (!reader.parse(dict_begin, s + 1, *value,
      // /*collectComments=*/false)) {
      //  PERFETTO_ELOG("JSON error: %s",
      //                reader.getFormattedErrorMessages().c_str());
      //  return kFatalError;
      //}
      *next = s + 1;
      return kFoundDict;
    }
    // TODO(primiano): skip braces in quoted strings, e.g.: {"foo": "ba{z" }
  }
  return kNeedsMoreData;
}

}  // namespace

// Json trace event timestamps are in us.
// https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/edit#heading=h.nso4gcezn7n1
base::Optional<int64_t> CoerceToNs(const sajson::value& value) {
  switch (static_cast<size_t>(value.get_type())) {
    case sajson::TYPE_DOUBLE:
      return static_cast<int64_t>(value.get_double_value() * 1000);
    case sajson::TYPE_INTEGER: {
      int64_t result;
      if (value.get_int53_value(&result))
        return result * 1000;
      return base::nullopt;
    }
    case sajson::TYPE_STRING: {
      char* end;
      int64_t n = strtoll(value.as_cstring(), &end, 10);
      if (end != value.as_cstring() + value.get_string_length())
        return base::nullopt;
      return n * 1000;
    }
    default:
      return base::nullopt;
  }
}

base::Optional<int64_t> CoerceToInt64(const sajson::value& value) {
  switch (static_cast<size_t>(value.get_type())) {
    case sajson::TYPE_INTEGER:
      int64_t result;
      if (value.get_int53_value(&result))
        return result * 1000;
      return base::nullopt;
    case sajson::TYPE_STRING: {
      char* end;
      int64_t n = strtoll(value.as_cstring(), &end, 10);
      if (end != value.as_cstring() + value.get_string_length())
        return base::nullopt;
      return n;
    }
    default:
      return base::nullopt;
  }
}

base::Optional<uint32_t> CoerceToUint32(const sajson::value& value) {
  base::Optional<int64_t> result = CoerceToInt64(value);
  if (!result.has_value())
    return base::nullopt;
  int64_t n = result.value();
  if (n < 0 || n > std::numeric_limits<uint32_t>::max())
    return base::nullopt;
  return static_cast<uint32_t>(n);
}

JsonTraceParser::JsonTraceParser(TraceProcessorContext* context)
    : context_(context) {}

JsonTraceParser::~JsonTraceParser() = default;

bool JsonTraceParser::Parse(std::unique_ptr<uint8_t[]> data, size_t size) {
  buffer_.insert(buffer_.end(), data.get(), data.get() + size);
  char* buf = &buffer_[0];
  const char* next = buf;
  const char* end = &buffer_[buffer_.size()];

  if (offset_ == 0) {
    // Trace could begin in any of these ways:
    // {"traceEvents":[{
    // { "traceEvents": [{
    // [{
    // Skip up to the first '['
    while (next != end && *next != '[') {
      next++;
    }
    if (next == end) {
      PERFETTO_ELOG("Failed to parse: first chunk missing opening [");
      return false;
    }
    next++;
  }

  ProcessTracker* procs = context_->process_tracker.get();
  TraceStorage* storage = context_->storage.get();
  SliceTracker* slice_tracker = context_->slice_tracker.get();

  while (next < end) {
    Json::Value value;
    const char* start;
    const auto res = ReadOneJsonDict(next, end, &start, &next);
    if (res == kEndOfTrace || res == kNeedsMoreData)
      break;
    sajson::string mut_input(start, static_cast<size_t>(next - start));
    sajson::document doc(sajson::parse(sajson::single_allocation(), mut_input));
    if (!doc.is_valid()) {
      PERFETTO_ELOG("Invalid!");
      return false;
    }
    sajson::value root = doc.get_root();
    if (root.get_type() != sajson::TYPE_OBJECT) {
      PERFETTO_ELOG("not obj!");
      return false;
    }

    const auto& ph = root.get_value_of_key(sajson::literal("ph"));
    if (ph.get_type() != sajson::TYPE_STRING)
      continue;
    char phase = *ph.as_cstring();

    base::Optional<uint32_t> opt_pid =
        CoerceToUint32(root.get_value_of_key(sajson::literal("pid")));
    base::Optional<uint32_t> opt_tid =
        CoerceToUint32(root.get_value_of_key(sajson::literal("tid")));

    uint32_t pid = opt_pid.value_or(0);
    uint32_t tid = opt_tid.value_or(pid);

    base::Optional<int64_t> opt_ts =
        CoerceToNs(root.get_value_of_key(sajson::literal("ts")));
    PERFETTO_CHECK(opt_ts.has_value());
    int64_t ts = opt_ts.value();

    const auto& cat_value = root.get_value_of_key(sajson::literal("cat"));
    const auto& name_value = root.get_value_of_key(sajson::literal("name"));
    const char* cat = (cat_value.get_type() == sajson::TYPE_STRING)
                          ? cat_value.as_cstring()
                          : "";
    const char* name = (cat_value.get_type() == sajson::TYPE_STRING)
                           ? name_value.as_cstring()
                           : "";
    StringId cat_id = storage->InternString(cat);
    StringId name_id = storage->InternString(name);
    UniqueTid utid = procs->UpdateThread(tid, pid);

    switch (phase) {
      case 'B': {  // TRACE_EVENT_BEGIN.
        slice_tracker->Begin(ts, utid, cat_id, name_id);
        break;
      }
      case 'E': {  // TRACE_EVENT_END.
        slice_tracker->End(ts, utid, cat_id, name_id);
        break;
      }
      case 'X': {  // TRACE_EVENT (scoped event).

        base::Optional<int64_t> opt_dur =
            CoerceToNs(root.get_value_of_key(sajson::literal("dur")));
        if (!opt_dur.has_value())
          continue;
        slice_tracker->Scoped(ts, utid, cat_id, name_id, opt_dur.value());
        break;
      }
      case 'M': {  // Metadata events (process and thread names).
        if (strcmp(root.get_value_of_key(sajson::literal("name")).as_cstring(),
                   "thread_name") == 0) {
          const char* thread_name =
              root.get_value_of_key(sajson::literal("args"))
                  .get_value_of_key(sajson::literal("name"))
                  .as_cstring();
          auto thrad_name_id = context_->storage->InternString(thread_name);
          procs->UpdateThread(ts, tid, thrad_name_id);
          break;
        }
        if (strcmp(root.get_value_of_key(sajson::literal("name")).as_cstring(),
                   "process_name") == 0) {
          const char* proc_name = root.get_value_of_key(sajson::literal("args"))
                                      .get_value_of_key(sajson::literal("name"))
                                      .as_cstring();
          procs->UpdateProcess(pid, base::nullopt, proc_name);
          break;
        }
      }
    }
  }
  offset_ += static_cast<uint64_t>(next - buf);
  buffer_.erase(buffer_.begin(), buffer_.begin() + (next - buf));
  return true;
}

}  // namespace trace_processor
}  // namespace perfetto
