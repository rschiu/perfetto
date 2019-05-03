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

#include "src/trace_processor/export_json.h"

#include <sstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <json/reader.h>
#include <json/value.h>

namespace perfetto {
namespace trace_processor {
namespace json {
namespace {

TEST(ExportJsonTest, EmptyStorage) {
  TraceStorage storage;

  std::ostringstream stream;
  ExportJson(&storage, stream);

  Json::Reader reader;
  Json::Value result;
  EXPECT_TRUE(reader.parse(stream.str(), result));
  EXPECT_EQ(result["traceEvents"].size(), 0);
}

TEST(ExportJsonTest, StorageWithOneSlice) {
  const int64_t kTimestamp = 10000000;
  const int64_t kDuration = 10000;
  const int64_t kThreadID = 100;
  const char* kCategory = "cat";
  const char* kName = "name";

  TraceStorage storage;
  UniqueTid utid = storage.AddEmptyThread(kThreadID);
  StringId cat_id = storage.InternString(base::StringView(kCategory));
  StringId name_id = storage.InternString(base::StringView(kName));
  storage.mutable_nestable_slices()->AddSlice(
      kTimestamp, kDuration, utid, RefType::kRefUtid, cat_id, name_id, 0, 0, 0);

  std::ostringstream stream;
  ExportJson(&storage, stream);

  Json::Reader reader;
  Json::Value result;
  EXPECT_TRUE(reader.parse(stream.str(), result));
  EXPECT_EQ(result["traceEvents"].size(), 1);

  Json::Value event = result["traceEvents"][0];
  EXPECT_EQ(event["ph"].asString(), "X");
  EXPECT_EQ(event["ts"].asInt64(), kTimestamp / 1000);
  EXPECT_EQ(event["duration"].asInt64(), kDuration / 1000);
  EXPECT_EQ(event["tid"].asUInt(), kThreadID);
  EXPECT_EQ(event["category"].asString(), kCategory);
  EXPECT_EQ(event["name"].asString(), kName);
}

TEST(ExportJsonTest, StorageWithThreadName) {
  const int64_t kThreadID = 100;
  const char* kName = "thread";

  TraceStorage storage;
  UniqueTid utid = storage.AddEmptyThread(kThreadID);
  StringId name_id = storage.InternString(base::StringView(kName));
  storage.GetMutableThread(utid)->name_id = name_id;

  std::ostringstream stream;
  ExportJson(&storage, stream);

  Json::Reader reader;
  Json::Value result;
  EXPECT_TRUE(reader.parse(stream.str(), result));
  EXPECT_EQ(result["traceEvents"].size(), 1);

  Json::Value event = result["traceEvents"][0];
  EXPECT_EQ(event["ph"].asString(), "M");
  EXPECT_EQ(event["tid"].asUInt(), kThreadID);
  EXPECT_EQ(event["name"].asString(), "thread_name");
  EXPECT_EQ(event["args"]["name"].asString(), kName);
}

}  // namespace
}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto
