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

#ifndef SRC_BASE_FALLTHROUGH_H_
#define SRC_BASE_FALLTHROUGH_H_

// NOTE: Do NOT use this macro in any of perfetto's public headers: When clang
// suggests inserting [[clang::fallthrough]], it first checks if it knows of a
// macro expanding to it, and if so suggests inserting the macro. This means
// that this macro must be used only in code internal to perfetto, so that
// embedder code doesn't end up getting suggestions for PERFETTO_FALLTHROUGH
// instead of the embedder-specific fallthrough macro.
#if defined(__clang__)
#define PERFETTO_FALLTHROUGH [[clang::fallthrough]]
#else
#define PERFETTO_FALLTHROUGH
#endif

#endif  // SRC_BASE_FALLTHROUGH_H_
