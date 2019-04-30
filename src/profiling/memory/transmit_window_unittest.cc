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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace perfetto {
namespace profiling {
namespace {

using ::testing::InvokeWithoutArgs;

class DummyDelegate : public TransmitWindowReceiver::Delegate {
 public:
  MOCK_METHOD2(AcknowledgeTransmitWindow, void(pid_t, size_t));
};

TEST(TransmitWindowTest, Simple) {
  const pid_t pid = 1234;

  TransmitWindowSender sender(2);

  DummyDelegate delegate;

  TransmitWindowReceiver receiver(pid, &delegate, 1);

  EXPECT_TRUE(sender.Send());
  EXPECT_FALSE(sender.Send());
  EXPECT_CALL(delegate, AcknowledgeTransmitWindow(pid, 1))
      .WillOnce(InvokeWithoutArgs([&sender] { sender.Acknowledge(1); }));
  receiver.Receive();
  EXPECT_FALSE(sender.Send());
  EXPECT_CALL(delegate, AcknowledgeTransmitWindow(pid, 1))
      .Times(2)
      .WillRepeatedly(InvokeWithoutArgs([&sender] { sender.Acknowledge(1); }));
  receiver.Receive();
  receiver.Receive();
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
