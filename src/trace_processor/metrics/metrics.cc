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

#include "src/trace_processor/metrics/metrics.h"

#include <unordered_map>
#include <vector>

#include "perfetto/metrics/android/mem_metric.pbzero.h"
#include "perfetto/metrics/metrics.pbzero.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/trace_processor/metrics/sql_metrics.h"

namespace perfetto {
namespace trace_processor {
namespace metrics {

namespace {

std::vector<std::string> SplitString(const std::string& text,
                                     const std::string& delimiter) {
  std::vector<std::string> output;
  size_t start = 0;
  for (size_t i = 0; i < text.size(); i++) {
    bool matches = true;
    size_t j = i;
    for (char c : delimiter) {
      if (text[j++] != c) {
        matches = false;
        break;
      }
    }
    if (matches) {
      output.emplace_back(&text[start], i - start);
      start = j;
    }
  }
  if (start < text.size())
    output.emplace_back(&text[start], text.size() - start);
  return output;
}

int TemplateReplace(
    const std::string& raw_text,
    const std::unordered_map<std::string, std::string>& substituitions,
    std::vector<char>* out) {
  int64_t sub_start_idx = -1;
  for (size_t i = 0; i < raw_text.size(); i++) {
    char c = raw_text[i];
    if (c == '{') {
      if (sub_start_idx != -1)
        return 1;

      if (i + 1 < raw_text.size() && raw_text[i + 1] == '{') {
        sub_start_idx = static_cast<int64_t>(i) + 2;
        i++;
        continue;
      }
    } else if (c == '}' && sub_start_idx != -1) {
      if (i + 1 >= raw_text.size() || raw_text[i] != '}')
        return 2;

      size_t len = i - static_cast<size_t>(sub_start_idx);
      auto key = base::StringView(&(raw_text.data()[sub_start_idx]), len)
                     .ToStdString();
      const auto& value = substituitions.find(key)->second;
      std::copy(value.begin(), value.end(), std::back_inserter(*out));

      sub_start_idx = -1;
      i++;
      continue;
    } else if (sub_start_idx != -1) {
      continue;
    }
    out->push_back(c);
  }
  out->push_back('\0');
  return sub_start_idx == -1 ? 0 : 3;
}

struct FunctionContext {
  TraceProcessorImpl* tp;
};

void RunMetric(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  auto* fn_ctx = static_cast<FunctionContext*>(sqlite3_user_data(ctx));
  if (argc % 2 != 1 || sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
    sqlite3_result_error(ctx, "Invalid call to RUN_METRIC", -1);
    return;
  }

  const char* filename =
      reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
  const char* sql = sql_metrics::FindSqlFromFilename(filename);
  if (!sql) {
    sqlite3_result_error(ctx, "RUN_METRIC: Unknown filename provided", -1);
    return;
  }

  std::unordered_map<std::string, std::string> substitutions;
  for (int i = 1; i < argc; i += 2) {
    if (sqlite3_value_type(argv[i]) != SQLITE_TEXT) {
      sqlite3_result_error(ctx, "RUN_METRIC: Invalid args", -1);
      return;
    }

    auto* key_str = reinterpret_cast<const char*>(sqlite3_value_text(argv[i]));
    auto* value_str =
        reinterpret_cast<const char*>(sqlite3_value_text(argv[i + 1]));
    substitutions[key_str] = value_str;
  }

  for (const auto& query : SplitString(sql, ";\n")) {
    std::vector<char> buffer;
    int ret = TemplateReplace(query, substitutions, &buffer);
    if (ret) {
      sqlite3_result_error(
          ctx, "RUN_METRIC: Error when performing substitution", -1);
      return;
    }

    PERFETTO_LOG("RUN_METRIC: Executing query: %s", buffer.data());
    auto it = fn_ctx->tp->ExecuteQuery(buffer.data());
    if (auto opt_error = it.GetLastError()) {
      sqlite3_result_error(ctx, "RUN_METRIC: Error when running file", -1);
      return;
    } else if (it.Next()) {
      sqlite3_result_error(ctx, "RUN_METRIC: function produced output", -1);
      return;
    }
  }
}

}  // namespace

int ComputeMetrics(TraceProcessorImpl* tp,
                   const std::vector<std::string>& metric_names,
                   std::vector<uint8_t>* metrics_proto) {
  if (metric_names.size() != 1 || metric_names[0] != "android.mem") {
    PERFETTO_ELOG("Only android.mem metric is currently supported");
    return 1;
  }

  std::unique_ptr<FunctionContext> ctx(new FunctionContext());
  ctx->tp = tp;
  auto ret =
      tp->RegisterScalarFunction("RUN_METRIC", -1, std::move(ctx), &RunMetric);
  if (ret) {
    PERFETTO_ELOG("SQLite error: %d", ret);
    return ret;
  }

  for (const auto& query : SplitString(sql_metrics::kAndroidMem, ";\n")) {
    PERFETTO_DLOG("Executing query: %s", query.c_str());
    auto prep_it = tp->ExecuteQuery(query);
    auto prep_has_next = prep_it.Next();
    if (auto opt_error = prep_it.GetLastError()) {
      PERFETTO_ELOG("SQLite error: %s", opt_error->c_str());
      return 1;
    }
    PERFETTO_DCHECK(!prep_has_next);
  }

  protozero::ScatteredHeapBuffer delegate;
  protozero::ScatteredStreamWriter writer(&delegate);
  delegate.set_writer(&writer);

  protos::pbzero::TraceMetrics metrics;
  metrics.Reset(&writer);

  auto it = tp->ExecuteQuery("SELECT COUNT(*) from lmk_by_score;");
  auto has_next = it.Next();
  if (auto opt_error = it.GetLastError()) {
    PERFETTO_ELOG("SQLite error: %s", opt_error->c_str());
    return 1;
  }
  PERFETTO_CHECK(has_next);
  PERFETTO_CHECK(it.Get(0).type == SqlValue::Type::kLong);

  has_next = it.Next();
  PERFETTO_DCHECK(!has_next);

  auto* memory = metrics.set_android_mem();
  memory->set_system_metrics()->set_lmks()->set_total_count(
      static_cast<int32_t>(it.Get(0).long_value));

  it = tp->ExecuteQuery("SELECT * from anon_rss;");
  while (it.Next()) {
    const char* name = it.Get(0).string_value;

    auto* process = memory->add_process_metrics();
    process->set_process_name(name);

    auto* anon = process->set_overall_counters()->set_anon_rss();
    anon->set_min(it.Get(1).AsDouble());
    anon->set_max(it.Get(2).AsDouble());
    anon->set_avg(it.Get(3).AsDouble());
  }
  if (auto opt_error = it.GetLastError()) {
    PERFETTO_ELOG("SQLite error: %s", opt_error->c_str());
    return 1;
  }

  metrics.Finalize();
  *metrics_proto = delegate.StitchSlices();
  return 0;
}

}  // namespace metrics
}  // namespace trace_processor
}  // namespace perfetto
