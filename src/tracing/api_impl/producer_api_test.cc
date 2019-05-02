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

#include <inttypes.h>
#include <stdint.h>

#include <array>
#include <atomic>

#include "perfetto/public/producer_api.h"
#include "perfetto/trace/trace_packet.pbzero.h"

// TODO: how do we avoid this dependency?
#include "perfetto/tracing/core/data_source_descriptor.h"

// using namespace perfetto;

namespace {

class MyDataSource : public perfetto::DataSource<MyDataSource> {
  // static void GetDescriptor(perfetto::DataSourceDescriptor* desc);

  void OnSetup(const perfetto::DataSourceConfig&) override {}
  void OnStart() override {}
  void OnStop() override {}
};

}  // namespace

// TODO: this needs to be hidden behind a macro, like INSTANTIATE_DATA_SOURCE(MyDataSource).
template <>
perfetto::internal::DataSourceStaticState perfetto::DataSource<MyDataSource>::static_state_{};

int main() {
  perfetto::Context::InitArgs args;
  args.mode = perfetto::Context::InitArgs::Mode::kSystem;
  perfetto::Context::Initialize(args);

  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("my_data_source");
  perfetto::Context::Get()->RegisterDataSource<MyDataSource>(dsd);

  using TPacket = perfetto::protos::pbzero::TracePacket;
  MyDataSource::Trace<TPacket>([](TPacket* p) {
    p->set_trusted_uid(42);
  });

  return 0;
}
