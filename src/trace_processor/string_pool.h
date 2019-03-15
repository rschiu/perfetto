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

#ifndef SRC_TRACE_PROCESSOR_STRING_POOL_H_
#define SRC_TRACE_PROCESSOR_STRING_POOL_H_

#include "perfetto/base/paged_memory.h"
#include "src/trace_processor/null_term_string_view.h"

#include <unordered_map>
#include <vector>

namespace perfetto {
namespace trace_processor {

// Interns strings in a string pool and hands out compact StringIds which can
// be used to retrieve the string.
class StringPool {
 public:
  using Id = uint32_t;

  StringPool();
  ~StringPool() = default;

  // Allow std::move().
  StringPool(StringPool&&) noexcept = default;
  StringPool& operator=(StringPool&&) = default;

  // Disable implicit copy.
  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;

  Id InternString(base::StringView);

  PERFETTO_ALWAYS_INLINE NullTermStringView Get(Id id) const {
    uint8_t* ptr;
    if (sizeof(uint8_t*) == 8) {
      PERFETTO_DCHECK(blocks_.size() == 1);
      ptr = blocks_.back().Get(id);
    } else {
      ptr = reinterpret_cast<uint8_t*>(id);
    }
    // First two bytes hold the length of the string (little endian order).
    uint16_t lsb = ptr[0];
    uint16_t msb = ptr[1];
    uint16_t size = static_cast<uint16_t>(msb << 8u | lsb);
    return NullTermStringView(reinterpret_cast<char*>(ptr) + 2, size);
  }

  size_t size() const { return string_index_.size(); }

 private:
  using StringHash = uint64_t;
  struct Block {
    Block() : inner_(base::PagedMemory::Allocate(BlockSize())) {}
    ~Block() = default;

    // Allow std::move().
    Block(Block&&) noexcept = default;
    Block& operator=(Block&&) = default;

    // Disable implicit copy.
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;

    uint8_t* Get(uint32_t offset) const {
      return static_cast<uint8_t*>(inner_.Get()) + offset;
    }

    uint8_t* Reserve(uint32_t size) {
      if (pos_ + size >= BlockSize())
        return nullptr;
      uint32_t start = pos_;
      pos_ += size;
      return Get(start);
    }

    uint32_t OffsetOf(uint8_t* ptr) const {
      return static_cast<uint32_t>(ptr - Get(0));
    }

   private:
    static size_t BlockSize() {
      if (sizeof(uint8_t*) == 8)
        return 4ull * 1024ull * 1024ull * 1024ull;  // 4GB
      return 32ull * 1024ull * 1024ull;             // 32MB
    }

    base::PagedMemory inner_;
    uint32_t pos_ = 0;
  };

  // The actual memory storing the strings.
  std::vector<Block> blocks_;

  // Maps hashes of strings to the Id in the string pool.
  std::unordered_map<StringHash, Id> string_index_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_STRING_POOL_H_
