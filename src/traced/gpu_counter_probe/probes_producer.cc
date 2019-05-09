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

#include "src/traced/gpu_counter_probe/probes_producer.h"

#include <stdio.h>
#include <sys/stat.h>

#include <algorithm>
#include <queue>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/utils.h"
#include "perfetto/base/weak_ptr.h"
#include "perfetto/traced/traced.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/ftrace_config.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/core/trace_packet.h"
#include "perfetto/tracing/ipc/producer_ipc_client.h"
#include "src/traced/gpu_counter_probe/gpu/android_gpu_data_source.h"

namespace perfetto {
namespace {

constexpr uint32_t kInitialConnectionBackoffMs = 100;
constexpr uint32_t kMaxConnectionBackoffMs = 30 * 1000;

// Should be larger than FtraceController::kFlushTimeoutMs.
constexpr uint32_t kFlushTimeoutMs = 1000;

constexpr char kGpuSourceName[] = "android.gpu";

}  // namespace.

// State transition diagram:
//                    +----------------------------+
//                    v                            +
// NotStarted -> NotConnected -> Connecting -> Connected
//                    ^              +
//                    +--------------+
//

ProbesProducer::ProbesProducer() : weak_factory_(this) {}
ProbesProducer::~ProbesProducer() {
  // The ftrace data sources must be deleted before the ftrace controller.
  data_sources_.clear();
  //ftrace_.reset();
}

void ProbesProducer::OnConnect() {
  PERFETTO_DCHECK(state_ == kConnecting);
  state_ = kConnected;
  ResetConnectionBackoff();
  PERFETTO_LOG("Connected to the service");

  {
    DataSourceDescriptor desc;
    desc.set_name(kGpuSourceName);
    endpoint_->RegisterDataSource(desc);
  }
}

void ProbesProducer::OnDisconnect() {
  PERFETTO_DCHECK(state_ == kConnected || state_ == kConnecting);
  PERFETTO_LOG("Disconnected from tracing service");
  if (state_ == kConnected)
    return task_runner_->PostTask([this] { this->Restart(); });

  state_ = kNotConnected;
  IncreaseConnectionBackoff();
  task_runner_->PostDelayedTask([this] { this->Connect(); },
                                connection_backoff_ms_);
}

void ProbesProducer::Restart() {
  // We lost the connection with the tracing service. At this point we need
  // to reset all the data sources. Trying to handle that manually is going to
  // be error prone. What we do here is simply desroying the instance and
  // recreating it again.
  // TODO(hjd): Add e2e test for this.

  base::TaskRunner* task_runner = task_runner_;
  const char* socket_name = socket_name_;

  // Invoke destructor and then the constructor again.
  this->~ProbesProducer();
  new (this) ProbesProducer();

  ConnectWithRetries(socket_name, task_runner);
}

void ProbesProducer::SetupDataSource(DataSourceInstanceID instance_id,
                                     const DataSourceConfig& config) {
  PERFETTO_DLOG("SetupDataSource(id=%" PRIu64 ", name=%s)", instance_id,
                config.name().c_str());
  PERFETTO_DCHECK(data_sources_.count(instance_id) == 0);
  TracingSessionID session_id = config.tracing_session_id();
  PERFETTO_CHECK(session_id > 0);

  std::unique_ptr<ProbesDataSource> data_source;
  if (config.name() == kGpuSourceName) {
    data_source = CreateGpuDataSource(session_id, config);
  }

  if (!data_source) {
    PERFETTO_ELOG("Failed to create data source '%s'", config.name().c_str());
    return;
  }

  session_data_sources_.emplace(session_id, data_source.get());
  data_sources_[instance_id] = std::move(data_source);
}

std::unique_ptr<ProbesDataSource> ProbesProducer::CreateGpuDataSource(
    TracingSessionID session_id,
    const DataSourceConfig& source_config) {
  PERFETTO_LOG("GPU data setup (target_buf=%" PRIu32 ")",
               source_config.target_buffer());
  auto buffer_id = static_cast<BufferID>(source_config.target_buffer());
  return std::unique_ptr<AndroidGpuDataSource>(new AndroidGpuDataSource(
      std::move(source_config), task_runner_, session_id, endpoint_->CreateTraceWriter(buffer_id)));
}

void ProbesProducer::StartDataSource(DataSourceInstanceID instance_id,
                                     const DataSourceConfig& config) {
  PERFETTO_DLOG("StartDataSource(id=%" PRIu64 ", name=%s)", instance_id,
                config.name().c_str());
  auto it = data_sources_.find(instance_id);
  if (it == data_sources_.end()) {
    // Can happen if SetupDataSource() failed (e.g. ftrace was busy).
    PERFETTO_ELOG("Data source id=%" PRIu64 " not found", instance_id);
    return;
  }
  ProbesDataSource* data_source = it->second.get();
  if (data_source->started)
    return;
  if (config.trace_duration_ms() != 0) {
    uint32_t timeout = 5000 + 2 * config.trace_duration_ms();
    watchdogs_.emplace(
        instance_id, base::Watchdog::GetInstance()->CreateFatalTimer(timeout));
  }
  data_source->started = true;
  data_source->Start();
}

void ProbesProducer::StopDataSource(DataSourceInstanceID id) {
  PERFETTO_LOG("Producer stop (id=%" PRIu64 ")", id);
  auto it = data_sources_.find(id);
  if (it == data_sources_.end()) {
    // Can happen if SetupDataSource() failed (e.g. ftrace was busy).
    PERFETTO_ELOG("Cannot stop data source id=%" PRIu64 ", not found", id);
    return;
  }
  ProbesDataSource* data_source = it->second.get();
  TracingSessionID session_id = data_source->tracing_session_id;
  auto range = session_data_sources_.equal_range(session_id);
  for (auto kv = range.first; kv != range.second; kv++) {
    if (kv->second != data_source)
      continue;
    session_data_sources_.erase(kv);
    break;
  }
  data_sources_.erase(it);
  watchdogs_.erase(id);
}

void ProbesProducer::OnTracingSetup() {}

void ProbesProducer::Flush(FlushRequestID flush_request_id,
                           const DataSourceInstanceID* data_source_ids,
                           size_t num_data_sources) {
  PERFETTO_DCHECK(flush_request_id);
  auto weak_this = weak_factory_.GetWeakPtr();

  // Issue a Flush() to all started data sources.
  bool flush_queued = false;
  for (size_t i = 0; i < num_data_sources; i++) {
    DataSourceInstanceID ds_id = data_source_ids[i];
    auto it = data_sources_.find(ds_id);
    if (it == data_sources_.end() || !it->second->started)
      continue;
    pending_flushes_.emplace(flush_request_id, ds_id);
    flush_queued = true;
    auto flush_callback = [weak_this, flush_request_id, ds_id] {
      if (weak_this)
        weak_this->OnDataSourceFlushComplete(flush_request_id, ds_id);
    };
    it->second->Flush(flush_request_id, flush_callback);
  }

  // If there is nothing to flush, ack immediately.
  if (!flush_queued) {
    endpoint_->NotifyFlushComplete(flush_request_id);
    return;
  }

  // Otherwise, post the timeout task.
  task_runner_->PostDelayedTask(
      [weak_this, flush_request_id] {
        if (weak_this)
          weak_this->OnFlushTimeout(flush_request_id);
      },
      kFlushTimeoutMs);
}

void ProbesProducer::OnDataSourceFlushComplete(FlushRequestID flush_request_id,
                                               DataSourceInstanceID ds_id) {
  PERFETTO_DLOG("Flush %" PRIu64 " acked by data source %" PRIu64,
                flush_request_id, ds_id);
  auto range = pending_flushes_.equal_range(flush_request_id);
  for (auto it = range.first; it != range.second; it++) {
    if (it->second == ds_id) {
      pending_flushes_.erase(it);
      break;
    }
  }

  if (pending_flushes_.count(flush_request_id))
    return;  // Still waiting for other data sources to ack.

  PERFETTO_DLOG("All data sources acked to flush %" PRIu64, flush_request_id);
  endpoint_->NotifyFlushComplete(flush_request_id);
}

void ProbesProducer::OnFlushTimeout(FlushRequestID flush_request_id) {
  if (pending_flushes_.count(flush_request_id) == 0)
    return;  // All acked.
  PERFETTO_ELOG("Flush(%" PRIu64 ") timed out", flush_request_id);
  pending_flushes_.erase(flush_request_id);
  endpoint_->NotifyFlushComplete(flush_request_id);
}

void ProbesProducer::ConnectWithRetries(const char* socket_name,
                                        base::TaskRunner* task_runner) {
  PERFETTO_DCHECK(state_ == kNotStarted);
  state_ = kNotConnected;

  ResetConnectionBackoff();
  socket_name_ = socket_name;
  task_runner_ = task_runner;
  Connect();
}

void ProbesProducer::Connect() {
  PERFETTO_DCHECK(state_ == kNotConnected);
  state_ = kConnecting;
  endpoint_ = ProducerIPCClient::Connect(
      socket_name_, this, "perfetto.traced_probes", task_runner_);
}

void ProbesProducer::IncreaseConnectionBackoff() {
  connection_backoff_ms_ *= 2;
  if (connection_backoff_ms_ > kMaxConnectionBackoffMs)
    connection_backoff_ms_ = kMaxConnectionBackoffMs;
}

void ProbesProducer::ResetConnectionBackoff() {
  connection_backoff_ms_ = kInitialConnectionBackoffMs;
}

}  // namespace perfetto
