// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {timeToCode} from './time';

test('seconds to code', () => {
  expect(timeToCode(3)).toEqual('3s ');
  expect(timeToCode(60)).toEqual('1m ');
  expect(timeToCode(63)).toEqual('1m 3s ');
  expect(timeToCode(63.2)).toEqual('1m 3s 200ms ');
  expect(timeToCode(63.2221)).toEqual('1m 3s 222ms 100μs ');
  expect(timeToCode(63.2221111)).toEqual('1m 3s 222ms 111μs ');
  expect(timeToCode(0.2221111)).toEqual('222ms 111μs ');
});
