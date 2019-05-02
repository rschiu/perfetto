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

#include "perfetto/public/producer_api.h"

#include <inttypes.h>

#include "perfetto/base/logging.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/producer.h"
#include "perfetto/tracing/core/tracing_service.h"

namespace perfetto {

namespace {

using FactoryMethod = internal::FactoryMethod;

Context* g_context = nullptr;

struct RegisteredDataSource {
  DataSourceDescriptor descriptor;
  FactoryMethod factory;
  internal::DataSourceStaticState* static_state_;
};

class ProducerImpl : public Producer {
 public:
  ~ProducerImpl() override = default;

  void RegisterDataSource(const DataSourceDescriptor&,
                          FactoryMethod,
                          internal::DataSourceStaticState*);

  // perfetto::Producer implementation.
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingSetup() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StopDataSource(DataSourceInstanceID) override;
  void Flush(FlushRequestID,
             const DataSourceInstanceID* data_source_ids,
             size_t num_data_sources) override;

 private:
  std::vector<RegisteredDataSource> registered_data_sources_;
};

class SystemContext : public Context {
 public:
  SystemContext();
  ~SystemContext() override = default;
  ProducerEndpoint* GetProducerEndpoint() override;
  Producer* GetProducer() override;
  void RegisterDataSourceInternal(const DataSourceDescriptor&,
                                  internal::FactoryMethod,
                                  internal::DataSourceStaticState*) override;

 private:
  ProducerImpl producer_;
};

SystemContext::SystemContext() {}

ProducerEndpoint* SystemContext::GetProducerEndpoint() {
  return nullptr;
}

Producer* SystemContext::GetProducer() {
  return &producer_;
}

void SystemContext::RegisterDataSourceInternal(
    const DataSourceDescriptor& dsd,
    internal::FactoryMethod factory_method,
    internal::DataSourceStaticState* static_state) {
  producer_.RegisterDataSource(dsd, factory_method, static_state);
}

void ProducerImpl::RegisterDataSource(
    const DataSourceDescriptor& dsd,
    FactoryMethod factory,
    internal::DataSourceStaticState* static_state) {
  registered_data_sources_.emplace_back(
      RegisteredDataSource{dsd, factory, static_state});
}

void ProducerImpl::OnConnect() {}

void ProducerImpl::OnDisconnect() {}

void ProducerImpl::OnTracingSetup() {}

void ProducerImpl::SetupDataSource(DataSourceInstanceID,
                                   const DataSourceConfig&) {}

void ProducerImpl::StartDataSource(DataSourceInstanceID instance_id,
                                   const DataSourceConfig& cfg) {
  for (const auto& rds : registered_data_sources_) {
    if (rds.descriptor.name() != cfg.name())
      continue;

    internal::DataSourceStaticState& static_state = *rds.static_state_;
    for (uint32_t i = 0; i < internal::kMaxSessions; i++) {
      if (static_state.enabled_sessions & (1 << i))
        continue;

      auto* session = new (static_state.sessions[i]) internal::Session{};

      {
        std::lock_guard<std::mutex> guard(session->lock);
        session->instance_id = instance_id;
        session->buffer_id = cfg.target_buffer();
        session->data_source = rds.factory();
      }
      // TODO: does this need to be a comp-xcgh? Shouldn't be accessedd
      // cross-thread though.
      static_state.enabled_sessions |= (1 << i);

      session->data_source->OnSetup(cfg);  // TODO: move to SetupDataSource.
      session->data_source->OnStart();
      break;
    }
  }
}

// TODO add thread checker to producerimpl.
void ProducerImpl::StopDataSource(DataSourceInstanceID instance_id) {
  PERFETTO_LOG("Stopping data source %" PRIu64, instance_id);
  // TODO: this could be optimized if we add a map<iid, session> but not sure
  // it's worth it.
  for (const auto& rds : registered_data_sources_) {
    internal::DataSourceStaticState& static_state = *rds.static_state_;
    for (uint32_t i = 0; i < internal::kMaxSessions; i++) {
      if (!(static_state.enabled_sessions & (1 << i)))
        continue;

      auto* session = static_state.sessions[i];
      std::lock_guard<std::mutex> guard(session->lock);

      if (session->instance_id != instance_id)
        continue;

      // TODO: risk of deadlock if the DS does something clever and re-enters
      // from within OnStop.
      session->data_source->OnStop();
      session->data_source->~DataSourceBase();
      static_state.enabled_sessions &= ~(1 << i);  // TODO compxcgh?
      return;
    }
  }
  PERFETTO_ELOG("Could not find data source to stop");
}

void ProducerImpl::Flush(FlushRequestID, const DataSourceInstanceID*, size_t) {
  // TODO: we can't really do this, have to rely on the service-side scraping.
  // Figure out how to enable that.
}

}  // namespace

Context::Context() = default;
Context::~Context() = default;

// static
Context* Context::Get() {
  PERFETTO_CHECK(g_context);
  return g_context;
};

// static
void Context::Initialize(const Context::InitArgs& args) {
  PERFETTO_CHECK(!g_context);
  if (args.existing_context) {
    g_context = args.existing_context;
    return;
  }

  g_context = new SystemContext();
}

DataSourceBase::~DataSourceBase() = default;

}  // namespace perfetto
