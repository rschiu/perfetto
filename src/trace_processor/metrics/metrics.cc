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

#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/trace_processor/metrics/metrics.descriptor.h"
#include "src/trace_processor/metrics/sql_metrics.h"

#include "perfetto/common/descriptor.pbzero.h"
#include "perfetto/metrics/android/mem_metric.pbzero.h"
#include "perfetto/metrics/metrics.pbzero.h"

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

class FieldDescriptor {
 public:
  FieldDescriptor(std::string name,
                  uint32_t number,
                  uint32_t type,
                  std::string raw_type_name,
                  bool is_repeated)
      : name_(std::move(name)),
        number_(number),
        type_(type),
        raw_type_name_(std::move(raw_type_name)),
        is_repeated_(is_repeated) {}

  void SetMessageTypeIdx(uint32_t idx) { message_type_idx_ = idx; }

  const std::string& name() const { return name_; }
  uint32_t number() const { return number_; }
  uint32_t type() const { return type_; }
  const std::string& raw_type_name() const { return raw_type_name_; }
  bool is_repeated() const { return is_repeated_; }

 private:
  std::string name_;
  uint32_t number_;
  uint32_t type_;
  std::string raw_type_name_;
  bool is_repeated_;

  base::Optional<uint32_t> message_type_idx_;
};

class ProtoDescriptor {
 public:
  ProtoDescriptor(std::string package_name,
                  std::string full_name,
                  base::Optional<uint32_t> parent_id)
      : package_name_(std::move(package_name)),
        full_name_(std::move(full_name)),
        parent_id_(parent_id) {}

  void AddField(FieldDescriptor descriptor) {
    fields_.emplace_back(std::move(descriptor));
  }

  base::Optional<uint32_t> FindFieldIdx(const std::string& name) const {
    auto it = std::find_if(
        fields_.begin(), fields_.end(),
        [name](const FieldDescriptor& desc) { return desc.name() == name; });
    auto idx = static_cast<uint32_t>(std::distance(fields_.begin(), it));
    return idx < fields_.size() ? base::Optional<uint32_t>(idx) : base::nullopt;
  }

  const std::string& package_name() const { return package_name_; }

  const std::string& full_name() const { return full_name_; }

  const std::vector<FieldDescriptor>& fields() const { return fields_; }
  std::vector<FieldDescriptor>* mutable_fields() { return &fields_; }

 private:
  std::string package_name_;
  std::string full_name_;
  base::Optional<uint32_t> parent_id_;
  std::vector<FieldDescriptor> fields_;
};

class DescriptorPool {
 public:
  uint32_t AddDescriptor(ProtoDescriptor descriptor) {
    descriptors_.emplace_back(std::move(descriptor));
    return static_cast<uint32_t>(descriptors_.size()) - 1;
  }

  base::Optional<uint32_t> FindDescriptorIdx(
      const std::string& full_name) const {
    auto it = std::find_if(descriptors_.begin(), descriptors_.end(),
                           [full_name](const ProtoDescriptor& desc) {
                             return desc.full_name() == full_name;
                           });
    auto idx = static_cast<uint32_t>(std::distance(descriptors_.begin(), it));
    return idx < descriptors_.size() ? base::Optional<uint32_t>(idx)
                                     : base::nullopt;
  }

  const std::vector<ProtoDescriptor>& descriptors() const {
    return descriptors_;
  }
  std::vector<ProtoDescriptor>* mutable_descriptors() { return &descriptors_; }

 private:
  std::vector<ProtoDescriptor> descriptors_;
};

void AddAllProtoDescriptors(const std::string& package_name,
                            base::Optional<uint32_t> parent_idx,
                            protos::pbzero::DescriptorProto::Decoder decoder,
                            DescriptorPool* pool) {
  auto parent_name =
      parent_idx ? pool->descriptors()[*parent_idx].full_name() : package_name;
  auto full_name =
      parent_name + "." + base::StringView(decoder.name()).ToStdString();

  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  ProtoDescriptor proto_descriptor(package_name, full_name, parent_idx);
  for (auto it = decoder.field(); it; ++it) {
    FieldDescriptorProto::Decoder f_decoder(it->data(), it->size());
    FieldDescriptor field(
        base::StringView(f_decoder.name()).ToStdString(),
        static_cast<uint32_t>(f_decoder.number()),
        static_cast<uint32_t>(f_decoder.type()),
        base::StringView(f_decoder.type_name()).ToStdString(),
        f_decoder.label() == FieldDescriptorProto::LABEL_REPEATED);
    proto_descriptor.AddField(std::move(field));
  }

  uint32_t idx = pool->AddDescriptor(std::move(proto_descriptor));
  for (auto it = decoder.nested_type(); it; ++it) {
    protos::pbzero::DescriptorProto::Decoder nested(it->data(), it->size());
    AddAllProtoDescriptors(package_name, idx, std::move(nested), pool);
  }
}

DescriptorPool GetMetricsProtoDescriptorPool() {
  DescriptorPool pool;

  // First pass: extract all the message descriptors from the file and add them
  // to the pool.
  protos::pbzero::FileDescriptorSet::Decoder proto(kMetricsDescriptor.data(),
                                                   kMetricsDescriptor.size());
  for (auto it = proto.file(); it; ++it) {
    protos::pbzero::FileDescriptorProto::Decoder file(it->data(), it->size());
    std::string package = "." + base::StringView(file.package()).ToStdString();

    using DescriptorProto = protos::pbzero::DescriptorProto;
    for (auto message_it = file.message_type(); message_it; ++message_it) {
      DescriptorProto::Decoder decoder(message_it->data(), message_it->size());
      AddAllProtoDescriptors(package, base::nullopt, std::move(decoder), &pool);
    }
  }

  // Second pass: resolve the types of all the fields to the correct indiices.
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  for (auto& descriptor : *pool.mutable_descriptors()) {
    for (auto& field : *descriptor.mutable_fields()) {
      if (field.type() == FieldDescriptorProto::TYPE_MESSAGE ||
          field.type() == FieldDescriptorProto::TYPE_ENUM) {
        field.SetMessageTypeIdx(
            pool.FindDescriptorIdx(field.raw_type_name()).value());
      }
    }
  }
  return pool;
}

struct RunMetricFunctionContext {
  TraceProcessorImpl* tp;
};

void RunMetric(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  auto* fn_ctx = static_cast<RunMetricFunctionContext*>(sqlite3_user_data(ctx));
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

    PERFETTO_DLOG("RUN_METRIC: Executing query: %s", buffer.data());
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

SqlValue SqlValueFromSqliteValue(sqlite3_value* value) {
  SqlValue sql_value;
  switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
      sql_value.type = SqlValue::Type::kLong;
      sql_value.long_value = sqlite3_value_int64(value);
      break;
    case SQLITE_FLOAT:
      sql_value.type = SqlValue::Type::kDouble;
      sql_value.double_value = sqlite3_value_double(value);
      break;
    case SQLITE_TEXT:
      sql_value.type = SqlValue::Type::kString;
      sql_value.string_value =
          reinterpret_cast<const char*>(sqlite3_value_text(value));
      break;
    case SQLITE_BLOB:
      sql_value.type = SqlValue::Type::kBytes;
      sql_value.bytes_value = sqlite3_value_blob(value);
      sql_value.bytes_count = static_cast<size_t>(sqlite3_value_bytes(value));
      break;
  }
  return sql_value;
}

struct BuildProtoFunctionContext {
  TraceProcessorImpl* tp;
  DescriptorPool* pool;
  std::string proto_name;
};

int AppendValueToMessage(sqlite3_context* ctx,
                         const FieldDescriptor& field,
                         const SqlValue& value,
                         protozero::Message* message) {
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  switch (field.type()) {
    case FieldDescriptorProto::TYPE_INT32:
    case FieldDescriptorProto::TYPE_INT64:
    case FieldDescriptorProto::TYPE_UINT32:
    case FieldDescriptorProto::TYPE_BOOL:
      if (value.type != SqlValue::kLong) {
        sqlite3_result_error(ctx, "BuildProto: field has wrong type", -1);
        return 1;
      }
      message->AppendVarInt(field.number(), value.long_value);
      break;
    case FieldDescriptorProto::TYPE_SINT32:
    case FieldDescriptorProto::TYPE_SINT64:
      if (value.type != SqlValue::kLong) {
        sqlite3_result_error(ctx, "BuildProto: field has wrong type", -1);
        return 1;
      }
      message->AppendSignedVarInt(field.number(), value.long_value);
      break;
    case FieldDescriptorProto::TYPE_FIXED32:
    case FieldDescriptorProto::TYPE_SFIXED32:
    case FieldDescriptorProto::TYPE_FIXED64:
    case FieldDescriptorProto::TYPE_SFIXED64:
      if (value.type != SqlValue::kLong) {
        sqlite3_result_error(ctx, "BuildProto: field has wrong type", -1);
        return 1;
      }
      message->AppendFixed(field.number(), value.long_value);
      break;
    case FieldDescriptorProto::TYPE_FLOAT:
    case FieldDescriptorProto::TYPE_DOUBLE:
      if (value.type != SqlValue::kDouble) {
        sqlite3_result_error(ctx, "BuildProto: field has wrong type", -1);
        return 1;
      }
      message->AppendFixed(field.number(), value.double_value);
      break;
    case FieldDescriptorProto::TYPE_STRING: {
      if (value.type != SqlValue::kString) {
        sqlite3_result_error(ctx, "BuildProto: field has wrong type", -1);
        return 1;
      }
      message->AppendString(field.number(), value.string_value);
      break;
    }
    case FieldDescriptorProto::TYPE_MESSAGE: {
      // TODO(lalitm): verify the type of the nested message.
      if (value.type != SqlValue::kBytes) {
        sqlite3_result_error(ctx, "BuildProto: field has wrong type", -1);
        return 1;
      }
      message->AppendBytes(field.number(), value.bytes_value,
                           value.bytes_count);
      break;
    }
    case FieldDescriptorProto::TYPE_UINT64:
      sqlite3_result_error(ctx, "BuildProto: uint64_t unsupported", -1);
      return 1;
    case FieldDescriptorProto::TYPE_GROUP:
      sqlite3_result_error(ctx, "BuildProto: groups unsupported", -1);
      return 1;
    case FieldDescriptorProto::TYPE_ENUM:
      // TODO(lalit): add support for enums.
      sqlite3_result_error(ctx, "BuildProto: enums unsupported", -1);
      return 1;
  }
  return 0;
}

int BuildProtoRepeatedField(sqlite3_context* ctx,
                            BuildProtoFunctionContext* fn_ctx,
                            const FieldDescriptor& field,
                            const std::string table_name,
                            protozero::Message* message) {
  std::string query = "SELECT * FROM " + table_name + ";";
  auto it = fn_ctx->tp->ExecuteQuery(query);
  while (it.Next()) {
    if (it.ColumnCount() != 1) {
      PERFETTO_ELOG("Repeated table should have exactly one column");
      return 1;
    }
    int ret = AppendValueToMessage(ctx, field, it.Get(0), message);
    if (ret)
      return ret;
  }
  if (auto opt_error = it.GetLastError()) {
    sqlite3_result_error(ctx, "BuildProtoRepeatedField error", -1);
    return 1;
  }
  return 0;
}

void BuildProto(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  auto* fn_ctx =
      static_cast<BuildProtoFunctionContext*>(sqlite3_user_data(ctx));
  if (argc % 2 != 0) {
    sqlite3_result_error(ctx, "Invalid call to BuildProto", -1);
    return;
  }

  auto opt_idx = fn_ctx->pool->FindDescriptorIdx(fn_ctx->proto_name);
  const auto& desc = fn_ctx->pool->descriptors()[opt_idx.value()];

  protozero::ScatteredHeapBuffer delegate;
  protozero::ScatteredStreamWriter writer(&delegate);
  delegate.set_writer(&writer);

  protozero::Message message;
  message.Reset(&writer);

  for (int i = 0; i < argc; i += 2) {
    auto* value = argv[i + 1];
    if (sqlite3_value_type(argv[i]) != SQLITE_TEXT) {
      sqlite3_result_error(ctx, "BuildProto: Invalid args", -1);
      return;
    }

    auto* key_str = reinterpret_cast<const char*>(sqlite3_value_text(argv[i]));
    auto opt_field_idx = desc.FindFieldIdx(key_str);
    const auto& field = desc.fields()[opt_field_idx.value()];
    if (field.is_repeated()) {
      if (sqlite3_value_type(value) != SQLITE_TEXT) {
        sqlite3_result_error(
            ctx, "BuildProto: repeated field doesn't have table name", -1);
        return;
      }
      auto* text = reinterpret_cast<const char*>(sqlite3_value_text(value));
      if (BuildProtoRepeatedField(ctx, fn_ctx, field, text, &message))
        return;
    } else {
      auto sql_value = SqlValueFromSqliteValue(value);
      if (AppendValueToMessage(ctx, field, sql_value, &message))
        return;
    }
  }
  message.Finalize();

  // TODO(lalitm): this is horribly inefficient but since our protos are small,
  // it will do for now. Change this to not have so many copies.
  auto slices = delegate.StitchSlices();
  std::unique_ptr<uint8_t[]> data(static_cast<uint8_t*>(malloc(slices.size())));
  memcpy(data.get(), slices.data(), slices.size());
  sqlite3_result_blob(ctx, data.release(), static_cast<int>(slices.size()),
                      free);
}

}  // namespace

int ComputeMetrics(TraceProcessorImpl* tp,
                   const std::vector<std::string>& metric_names,
                   std::vector<uint8_t>* metrics_proto) {
  if (metric_names.size() != 1 || metric_names[0] != "android.mem") {
    PERFETTO_ELOG("Only android.mem metric is currently supported");
    return 1;
  }

  std::unique_ptr<RunMetricFunctionContext> ctx(new RunMetricFunctionContext());
  ctx->tp = tp;
  auto ret =
      tp->RegisterScalarFunction("RUN_METRIC", -1, std::move(ctx), &RunMetric);
  if (ret) {
    PERFETTO_ELOG("SQLite error: %d", ret);
    return ret;
  }

  DescriptorPool pool = GetMetricsProtoDescriptorPool();
  for (const auto& desc : pool.descriptors()) {
    std::unique_ptr<BuildProtoFunctionContext> proto_fn_ctx(
        new BuildProtoFunctionContext());
    proto_fn_ctx->tp = tp;
    proto_fn_ctx->pool = &pool;
    proto_fn_ctx->proto_name = desc.full_name();

    // Convert the full name (e.g. .perfetto.protos.TraceMetrics.SubMetric)
    // into a function name of the form (TraceMetrics_SubMetric).
    auto fn_name = desc.full_name().substr(desc.package_name().size() + 1);
    std::replace(fn_name.begin(), fn_name.end(), '.', '_');

    ret = tp->RegisterScalarFunction(fn_name, -1, std::move(proto_fn_ctx),
                                     &BuildProto);
    if (ret) {
      PERFETTO_ELOG("SQLite error: %d", ret);
      return ret;
    }
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

  auto it = tp->ExecuteQuery("SELECT * from Output;");
  auto has_next = it.Next();
  if (auto opt_error = it.GetLastError()) {
    PERFETTO_ELOG("SQLite error: %s", opt_error->c_str());
    return 1;
  } else if (!has_next) {
    PERFETTO_ELOG("Output table should have at least one row");
    return 1;
  } else if (it.ColumnCount() != 1) {
    PERFETTO_ELOG("Output table should have exactly one column");
    return 1;
  } else if (it.Get(0).type != SqlValue::kBytes) {
    PERFETTO_ELOG("Output table column should have type bytes");
    return 1;
  }

  const uint8_t* ptr = static_cast<const uint8_t*>(it.Get(0).bytes_value);
  *metrics_proto = std::vector<uint8_t>(ptr, ptr + it.Get(0).bytes_count);

  has_next = it.Next();
  if (has_next) {
    PERFETTO_ELOG("Output table should only have one row");
    return 1;
  }

  return 0;
}

}  // namespace metrics
}  // namespace trace_processor
}  // namespace perfetto
