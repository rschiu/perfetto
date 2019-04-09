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

#include "src/trace_processor/syscall_tracker.h"

#include <utility>

#include <inttypes.h>

#include "src/trace_processor/slice_tracker.h"
#include "src/trace_processor/stats.h"

namespace perfetto {
namespace trace_processor {
namespace {

constexpr std::array<const char*, kSyscallCount> aarch64_to_syscall = {
#include "src/trace_processor/syscalls_aarch64.h"
};

constexpr std::array<const char*, kSyscallCount> aarch32_to_syscall = {
#include "src/trace_processor/syscalls_aarch32.h"
};

constexpr std::array<const char*, kSyscallCount> armeabi_to_syscall = {
#include "src/trace_processor/syscalls_armeabi.h"
};

constexpr std::array<const char*, kSyscallCount> x86_64_to_syscall = {
  #include "src/trace_processor/syscalls_x86_64.h"
};

// When we don't know the architecture map every syscall number to
// null string.
constexpr std::array<const char*, kSyscallCount> unknown_to_syscall = {{}};

}  // namespace

SyscallTracker::SyscallTracker(TraceProcessorContext* context)
    : context_(context) {
  // This sets arch_syscall_to_string_id_
  SetArchitecture(kUnknown);
}

SyscallTracker::~SyscallTracker() = default;

void SyscallTracker::SetArchitecture(Architecture arch) {
  const std::array<const char*, kSyscallCount>* arch_to_generic_syscall_number =
      nullptr;
  switch (arch) {
    case kAarch64:
      arch_to_generic_syscall_number = &aarch64_to_syscall;
      break;
    case kX86_64:
      arch_to_generic_syscall_number = &x86_64_to_syscall;
      break;
    case kUnknown:
      arch_to_generic_syscall_number = &unknown_to_syscall;
      break;
  }

  for (size_t i = 0; i < kSyscallCount; i++) {
    const char* name = (*arch_to_generic_syscall_number)[i];
    StringId id =
        context_->storage->InternString(name ? name : "UNKNOWN_SYSCALL");
    arch_syscall_to_string_id_[i] = id;
    if (name && !strcmp(name, "sys_write"))
      sys_write_string_id_ = id;
  }
}

void SyscallTracker::Enter(int64_t ts, UniqueTid utid, uint32_t syscall_num) {
  StringId name = SyscallNumberToStringId(syscall_num);
  if (!name) {
    context_->storage->IncrementStats(stats::sys_unknown_syscall);
    return;
  }

  context_->slice_tracker->Begin(ts, utid, 0 /* cat */, name);
}

void SyscallTracker::Exit(int64_t ts, UniqueTid utid, uint32_t syscall_num) {
  StringId name = SyscallNumberToStringId(syscall_num);
  if (!name) {
    context_->storage->IncrementStats(stats::sys_unknown_syscall);
    return;
  }

  context_->slice_tracker->End(ts, utid, 0 /* cat */, name);
}

}  // namespace trace_processor
}  // namespace perfetto
