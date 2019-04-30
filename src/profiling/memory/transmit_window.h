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

#ifndef SRC_PROFILING_MEMORY_TRANSMIT_WINDOW_H_
#define SRC_PROFILING_MEMORY_TRANSMIT_WINDOW_H_

#include <sys/types.h>

namespace perfetto {
namespace profiling {

class TransmitWindowSender {
 public:
  TransmitWindowSender(size_t send_threshold);
  bool Send();
  void Acknowledge(size_t size);

 private:
  const size_t send_threshold_;

  size_t send_unacknowledged_ = 0;
};

class TransmitWindowReceiver {
 public:
  class Delegate {
   public:
    virtual void AcknowledgeTransmitWindow(pid_t pid, size_t size) = 0;
    virtual ~Delegate();
  };

  TransmitWindowReceiver(pid_t pid, Delegate* delegate, size_t recv_threshold);
  void Receive();

 private:
  const pid_t pid_;
  Delegate* const delegate_;
  const size_t recv_threshold_;

  size_t recv_unacknowledged = 0;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_TRANSMIT_WINDOW_H_
