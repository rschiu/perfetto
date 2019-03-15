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

#include "src/trace_processor/string_pool.h"

namespace perfetto {
namespace trace_processor {

StringPool::StringPool() {
  blocks_.emplace_back();

  // Reserve one byte for the null string. This prevents any other string
  // being given id 0.
  PERFETTO_CHECK(blocks_.back().Reserve(1));
}

StringPool::Id StringPool::InternString(base::StringView str) {
  if (str.data() == nullptr)
    return 0;

  auto hash = str.Hash();
  auto id_it = string_index_.find(hash);
  if (id_it != string_index_.end()) {
    PERFETTO_DCHECK(Get(id_it->second) == str);
    return id_it->second;
  }

  // Number of bytes to reserve for size and null terminator.
  constexpr uint8_t kMetadataSize = 3;
  uint32_t str_size = static_cast<uint32_t>(str.size());
  auto* ptr = blocks_.back().Reserve(kMetadataSize + str_size);
  if (PERFETTO_UNLIKELY(!ptr)) {
    PERFETTO_DCHECK(sizeof(uint8_t*) == 4);
    blocks_.emplace_back();

    ptr = blocks_.back().Reserve(kMetadataSize + str_size);
    PERFETTO_CHECK(ptr);
  }

  // Write the size of the string (in little endian), followed by the string
  // itself finished by a null terminator.
  PERFETTO_DCHECK(str_size < std::numeric_limits<uint16_t>::max());
  ptr[0] = str_size & 0xFF;
  ptr[1] = (str_size & 0xFF00) >> 8;
  memcpy(&ptr[2], str.data(), str_size);
  ptr[2 + str_size] = '\0';

  Id string_id;
  if (sizeof(uint8_t*) == 8) {
    string_id = blocks_.back().OffsetOf(ptr);
  } else {
    // Double cast needed because on 64 archs, we get complaint that we are
    // losing information.
    string_id = static_cast<Id>(reinterpret_cast<uint64_t>(ptr));
  }

  string_index_.emplace(hash, string_id);
  return string_id;
}

}  // namespace trace_processor
}  // namespace perfetto
