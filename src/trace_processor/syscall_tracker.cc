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

// For the syscall with number n on aarch64 arch the nth entry on in the table
// is the same syscall in our platform independant syscall enum.
// http://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/
// https://thog.github.io/syscalls-table-aarch64/latest.html
constexpr std::array<Syscall, kSyscallCount> aarch64_to_syscall = {{
    kRestartSyscall,  //
    kSysExit,         //
    kUnknownSyscall,  //
    kSysRead,         //
    kSysWrite,        //
    kSysOpen,         //
    kSysClose,        //
}};

// For the syscall with number n on x86_64 arch the nth entry on in the table
// is the same syscall in our platform independant syscall enum.
// http://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/
constexpr std::array<Syscall, kSyscallCount> x86_64_to_syscall = {{
    kSysRead,   //
    kSysWrite,  //
    kSysOpen,   //
    kSysClose,  //
}};

// When we don't know the architecture map every syscall number to
// kUnknownSyscall.
constexpr std::array<Syscall, kSyscallCount> unknown_to_syscall = {{}};

}  // namespace

SyscallTracker::SyscallTracker(TraceProcessorContext* context)
    : context_(context) {
  InternSyscallString(kUnknownSyscall, "");
  InternSyscallString(kRestartSyscall, "restart_syscall");
  InternSyscallString(kSysExit, "sys_exit");
  InternSyscallString(kSysFork, "sys_fork");
  InternSyscallString(kSysRead, "sys_read");
  InternSyscallString(kSysWrite, "sys_write");
  InternSyscallString(kSysOpen, "sys_open");
  InternSyscallString(kSysClose, "sys_close");
  InternSyscallString(kSysCreat, "sys_creat");
  InternSyscallString(kSysLink, "sys_link");
  InternSyscallString(kSysUnlink, "sys_unlink");
  InternSyscallString(kSysExecve, "sys_execve");
  InternSyscallString(kSysChdir, "sys_chdir");
  InternSyscallString(kSysTime, "sys_time");

  // This sets arch_syscall_to_string_id_ and arch_to_generic_syscall_number_.
  SetArchitecture(kUnknown);
}

SyscallTracker::~SyscallTracker() = default;

void SyscallTracker::SetArchitecture(Architecture arch) {
  const std::array<Syscall, kSyscallCount>* arch_to_generic_syscall_number;
  switch (arch) {
    case kAarch64:
      arch_to_generic_syscall_number = &aarch64_to_syscall;
      break;
    case kX8664:
      arch_to_generic_syscall_number = &x86_64_to_syscall;
      break;
    case kUnknown:
      arch_to_generic_syscall_number = &unknown_to_syscall;
      break;
  }

  for (size_t i = 0; i < kSyscallCount; i++) {
    Syscall syscall = (*arch_to_generic_syscall_number)[i];
    arch_to_generic_syscall_number_[i] = syscall;
    arch_syscall_to_string_id_[i] = generic_syscall_to_string_id_[syscall];
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

void SyscallTracker::InternSyscallString(Syscall syscall,
                                         base::StringView view) {
  generic_syscall_to_string_id_[syscall] =
      context_->storage->InternString(view);
}

StringId SyscallTracker::SyscallNumberToStringId(uint32_t syscall_num) {
  if (syscall_num > kSyscallCount)
    return 0;
  // We see two write sys calls around each userspace slice that is going via
  // trace_marker, this violates the assumption that userspace slices are
  // perfectly nested. For the moment ignore all write sys calls.
  // TODO(hjd): Remove this limitation.
  if (arch_to_generic_syscall_number_[syscall_num] == kSysWrite)
    return 0;
  StringId sys_name_id = arch_syscall_to_string_id_[syscall_num];
  return sys_name_id;
}

}  // namespace trace_processor
}  // namespace perfetto
