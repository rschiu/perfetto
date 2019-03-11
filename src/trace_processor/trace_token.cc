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

#include "src/trace_processor/trace_token.h"

namespace perfetto {
namespace trace_processor {
TraceToken::TraceToken(Json::Value&& value) : type(TokenType::json_value) {
  // Placement new for in-place construction of union member.
  // Inspired by
  // http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Ru-anonymous
  new (&json_value) Json::Value();
  // Json::Value doesn't have a move constructor.
  json_value.swap(value);
}

TraceToken::TraceToken(TraceBlobView&& value) : type(TokenType::blob_view) {
  new (&blob_view) TraceBlobView(std::move(value));
}

TraceToken::TraceToken(TraceToken&& src) : type(src.type) {
  if (src.type == TokenType::json_value) {
    new (&json_value) Json::Value();
    json_value.swap(src.json_value);
  } else if (src.type == TokenType::blob_view) {
    new (&blob_view) TraceBlobView(std::move(src.blob_view));
  }
}

TraceToken& TraceToken::operator=(TraceToken&& rhs) {
  if (this != &rhs) {
    if (rhs.type == TokenType::json_value) {
      if (PERFETTO_UNLIKELY(this->type == TokenType::blob_view)) {
        this->type = rhs.type;
        blob_view.~TraceBlobView();
        new (&json_value) Json::Value();
      }
      json_value.swap(rhs.json_value);
    } else if (rhs.type == TokenType::blob_view) {
      if (PERFETTO_UNLIKELY(this->type == TokenType::json_value)) {
        this->type = rhs.type;
        json_value.~Value();
        new (&blob_view) TraceBlobView(std::move(rhs.blob_view));
      } else {
        blob_view = std::move(rhs.blob_view);
      }
    }
  }
  return *this;
}

// Need explicit destructors for union members.
// Inspired by
// http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Ru-anonymous
TraceToken::~TraceToken() {
  if (type == TokenType::json_value)
    json_value.~Value();
  else if (type == TokenType::blob_view)
    blob_view.~TraceBlobView();
}
}  // namespace trace_processor
}  // namespace perfetto
