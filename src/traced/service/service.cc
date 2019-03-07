/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "perfetto/base/unix_task_runner.h"
#include "perfetto/base/watchdog.h"
#include "perfetto/traced/traced.h"
#include "perfetto/tracing/ipc/service_ipc_host.h"
#include "src/tracing/ipc/default_socket.h"

// For the LazyProducer.
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/producer.h"
#include "perfetto/tracing/core/tracing_service.h"

namespace perfetto {

namespace {

// This should be its own class, whatever.
class LazyProducers : public Producer {
 public:
  void ConnectInProcess(TracingService* svc) {
    endpoint_ = svc->ConnectProducer(this, geteuid(), "lazy_producer",
                                     /*shm_hint_kb*/ 16);
  }

 private:
  void OnConnect() override {
    PERFETTO_ILOG("OnConnect");
    DataSourceDescriptor dsd;
    dsd.set_name("android.heapprofd");
    endpoint_->RegisterDataSource(dsd);
  }

  void OnDisconnect() override {}
  void OnTracingSetup() override {}
  void Flush(FlushRequestID, const DataSourceInstanceID*, size_t) override {}

  void SetupDataSource(DataSourceInstanceID id,
                       const DataSourceConfig&) override {
    PERFETTO_ILOG("Lazy setup %lld", id);
  }

  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override {
  }

  void StopDataSource(DataSourceInstanceID id) override {
    // Note: In the real impl give 30s or so before actually shutting down
    // heapprofd, so it can do any sort of cleanups. Also in case of a repeated
    // tracing sessions we don't keep respawning it.
    PERFETTO_ILOG("Lazy stop %lld", id);
  }

  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;
};

}  // namespace

int __attribute__((visibility("default"))) ServiceMain(int, char**) {
  base::UnixTaskRunner task_runner;
  std::unique_ptr<ServiceIPCHost> svc;
  svc = ServiceIPCHost::CreateInstance(&task_runner);
  LazyProducers lazy_prod;

  // When built as part of the Android tree, the two socket are created and
  // bonund by init and their fd number is passed in two env variables.
  // See libcutils' android_get_control_socket().
  const char* env_prod = getenv("ANDROID_SOCKET_traced_producer");
  const char* env_cons = getenv("ANDROID_SOCKET_traced_consumer");
  PERFETTO_CHECK((!env_prod && !env_cons) || (env_prod && env_cons));
  bool started;
  if (env_prod) {
    base::ScopedFile producer_fd(atoi(env_prod));
    base::ScopedFile consumer_fd(atoi(env_cons));
    started = svc->Start(std::move(producer_fd), std::move(consumer_fd));
  } else {
    unlink(GetProducerSocket());
    unlink(GetConsumerSocket());
    started = svc->Start(GetProducerSocket(), GetConsumerSocket());
  }

  if (!started) {
    PERFETTO_ELOG("Failed to start the traced service");
    return 1;
  }

  // Set the CPU limit and start the watchdog running. The memory limit will
  // be set inside the service code as it relies on the size of buffers.
  // The CPU limit is 75% over a 30 second interval.
  base::Watchdog* watchdog = base::Watchdog::GetInstance();
  watchdog->SetCpuLimit(75, 30 * 1000);
  watchdog->Start();

  PERFETTO_ILOG("Started traced, listening on %s %s", GetProducerSocket(),
                GetConsumerSocket());

  task_runner.PostTask([&svc, &lazy_prod] {
    lazy_prod.ConnectInProcess(svc->tracing_service());
  });

  task_runner.Run();
  return 0;
}

}  // namespace perfetto
