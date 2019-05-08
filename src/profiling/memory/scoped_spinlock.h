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

#ifndef SRC_PROFILING_MEMORY_SCOPED_SPINLOCK_H_
#define SRC_PROFILING_MEMORY_SCOPED_SPINLOCK_H_

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/base/utils.h"

#include <atomic>
#include <new>
#include <utility>

#include <inttypes.h>

namespace perfetto {
namespace profiling {

class ScopedSpinlock {
 public:
  enum class Mode {
    // Try for a fixed number of attempts, then return an unlocked handle.
    Try,
    // Keep spinning until successful.
    Blocking
  };

  ScopedSpinlock(std::atomic<bool>* lock, Mode mode) : lock_(lock) {
    struct timespec ts;
    if (PERFETTO_LIKELY(clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0)) {
      start_time_ = static_cast<uint64_t>(base::FromPosixTimespec(ts).count());
    }

    if (PERFETTO_LIKELY(!lock_->exchange(true, std::memory_order_acquire))) {
      locked_ = true;
      return;
    }
    LockSlow(mode);
  }

  ScopedSpinlock(const ScopedSpinlock&) = delete;
  ScopedSpinlock& operator=(const ScopedSpinlock&) = delete;

  ScopedSpinlock(ScopedSpinlock&& other) noexcept
      : lock_(other.lock_), locked_(other.locked_) {
    other.locked_ = false;
  }

  ScopedSpinlock& operator=(ScopedSpinlock&& other) {
    if (this != &other) {
      this->~ScopedSpinlock();
      new (this) ScopedSpinlock(std::move(other));
    }
    return *this;
  }

  ~ScopedSpinlock() { Unlock(); }

  void Unlock() {
    if (locked_) {
      PERFETTO_DCHECK(lock_->load());
      lock_->store(false, std::memory_order_release);
    }
    locked_ = false;

    struct timespec ts;
    if (start_time_ > 0 &&
        PERFETTO_UNLIKELY(clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0)) {
      uint64_t cur_time =
          static_cast<uint64_t>(base::FromPosixTimespec(ts).count());
      uint64_t diff_us = (cur_time - start_time_) / 1000;
      if (diff_us >= 5000)
        PERFETTO_LOG("DEBUG: Slow spinlock %p (> 5ms): %" PRIu64 " us.",
                     static_cast<void*>(lock_), diff_us);
    }
  }

  bool locked() const { return locked_; }

 private:
  void LockSlow(Mode mode);
  std::atomic<bool>* lock_;
  bool locked_ = false;
  uint64_t start_time_ = 0;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_SCOPED_SPINLOCK_H_
