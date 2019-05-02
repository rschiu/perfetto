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

#ifndef INCLUDE_PERFETTO_PUBLIC_PRODUCER_API_H_
#define INCLUDE_PERFETTO_PUBLIC_PRODUCER_API_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

// No perfetto headers should be leaked here.
// Only protozero is allowed.

namespace perfetto {

// TODO unnest the class from TracingService so it can be forward declared.
class ProducerEndpoint;
class Producer;
class DataSourceDescriptor;
class DataSourceConfig;

class DataSourceBase {
 public:
  virtual ~DataSourceBase();

  // TODO: introduce SetupArgs, StartArgs, StopArgs.

  virtual void OnSetup(const DataSourceConfig&) = 0;
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  void NewTracePacket();

 private:
  int buffer_id;
};

namespace internal {

// Max concurrent tracing sessions.
constexpr size_t kMaxSessions = 8;

struct Session {
  std::mutex lock;
  uint32_t buffer_id;
  uint64_t instance_id;
  std::unique_ptr<DataSourceBase> data_source;
};

struct DataSourceStaticState {
  std::atomic<uint32_t> enabled_sessions;
  std::array<Session*, kMaxSessions> sessions;

  void CompilerAssert() {
    static_assert(sizeof(enabled_sessions) * 8 >= kMaxSessions,
                  "kMasSessions too high");
  }
};

using FactoryMethod = std::function<std::unique_ptr<DataSourceBase>()>;

}  // namespace internal

// TODO can we avoid exposing the DSD argument? Can this be bytes?
// Think on how this can possibly be extended.
// virtual void GetDescriptor(DataSourceDescriptor*);

template <typename T>
class DataSource : public DataSourceBase {
 public:
  struct RegisterArgs {
    const char* name;
    const DataSourceDescriptor* descriptor;
  };

  template <typename PacketType, typename LambdaType>
  static void Trace(LambdaType l) {
    constexpr auto kMaxSessions = internal::kMaxSessions;
    auto sessions =
        static_state_.enabled_sessions.load(std::memory_order_relaxed);
    if (!sessions)
      return;
    for (uint32_t i = 0; i < kMaxSessions; i++) {
      if (!(sessions & (1 << i)))
        continue;

      PacketType m;
      l(&m);
    }
  }

  // Static state. Accessed by the static Trace() method.
  static internal::DataSourceStaticState static_state_;
};

class Context {
 public:
  struct InitArgs {
    enum class Mode { kInProcess = 0, kSystem };
    Mode mode = Mode::kInProcess;

    // Optional. If null the service will be created.
    Context* existing_context = nullptr;
  };

  // Returns the global context for the current process.
  // Context::Initialize() must have been called before or this will crash.
  static Context* Get();

  static void Initialize(const InitArgs&);

  // -----------------
  // Data Source Layer
  // -----------------
  template <typename DataSourceType>
  void RegisterDataSource(const DataSourceDescriptor& dsd) {
    auto factory = [] {
      return std::unique_ptr<DataSourceBase>(new DataSourceType());
    };
    RegisterDataSourceInternal(dsd, factory, &DataSourceType::static_state_);
  }

  virtual void RegisterDataSourceInternal(const DataSourceDescriptor&,
                                          internal::FactoryMethod,
                                          internal::DataSourceStaticState*) = 0;

  // Layer2
  virtual ProducerEndpoint* GetProducerEndpoint() = 0;
  virtual Producer* GetProducer() = 0;

 protected:
  Context();
  virtual ~Context();
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_PUBLIC_PRODUCER_API_H_
