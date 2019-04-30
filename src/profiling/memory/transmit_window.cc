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

#include "src/profiling/memory/transmit_window.h"

#include "perfetto/base/logging.h"

namespace perfetto {
namespace profiling {

TransmitWindowSender::TransmitWindowSender(size_t send_threshold)
    : send_threshold_(send_threshold) {}

bool TransmitWindowSender::Send() {
  return ++send_unacknowledged_ < send_threshold_;
}

void TransmitWindowSender::Acknowledge(size_t size) {
  PERFETTO_DCHECK(send_unacknowledged_ >= size);
  send_unacknowledged_ -= size;
}

TransmitWindowReceiver::TransmitWindowReceiver(pid_t pid,
                                               Delegate* delegate,
                                               size_t recv_threshold)
    : pid_(pid), delegate_(delegate), recv_threshold_(recv_threshold) {}

TransmitWindowReceiver::Delegate::~Delegate() = default;
void TransmitWindowReceiver::Receive() {
  if (++recv_unacknowledged == recv_threshold_) {
    delegate_->AcknowledgeTransmitWindow(pid_, recv_unacknowledged);
    recv_unacknowledged = 0;
  }
}

}  // namespace profiling
}  // namespace perfetto
