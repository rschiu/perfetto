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

#include "src/traced/gpu_counter_probe/gpu/android_gpu_data_source.h"

#include <dlfcn.h>

#include <vector>

#include <linux/types.h>
#include <linux/ioctl.h>

#include "perfetto/base/logging.h"
#include "perfetto/base/optional.h"
#include "perfetto/base/scoped_file.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/time.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/trace_packet.h"
#include "perfetto/tracing/core/trace_writer.h"

#include "perfetto/trace/gpu/gpu_stats.pbzero.h"
#include "perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

AndroidGpuDataSource::AndroidGpuDataSource(
    DataSourceConfig ,
    base::TaskRunner* task_runner,
    TracingSessionID session_id,
    std::unique_ptr<TraceWriter> writer)
    : ProbesDataSource(session_id, kTypeId),
      task_runner_(task_runner),
      poll_rate_ms_(1),
      writer_(std::move(writer)),
      weak_factory_(this) {
  // Place initialization code here.
}

AndroidGpuDataSource::~AndroidGpuDataSource() {
  // Place cleanup code here.
}

void AndroidGpuDataSource::Start() {
  Tick();
}

void AndroidGpuDataSource::Tick() {
  // Post next task.
  auto now_ms = base::GetWallTimeMs().count();
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this] {
        if (weak_this)
          weak_this->Tick();
      },
      poll_rate_ms_ - (now_ms % poll_rate_ms_));

  WriteGpuCounters();
}

void AndroidGpuDataSource::WriteGpuCounters() {
  static int64_t value = 0;
  auto packet = writer_->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));
  auto* gpu_stats = packet->set_gpu_stats();
  auto* counters = gpu_stats->add_counters();
  counters->set_counter_id(1);
  counters->set_value(value);
  counters = gpu_stats->add_counters();
  counters->set_counter_id(42);
  counters->set_value(2 * value);
  ++value;
}

void AndroidGpuDataSource::Flush(FlushRequestID,
                                   std::function<void()> callback) {
  writer_->Flush(callback);
}

}  // namespace perfetto
