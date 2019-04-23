--
-- Copyright 2019 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--

-- Create all the views used to generate the Android Memory metrics proto.
SELECT RUN_METRIC('android_mem_lmk.sql');

-- Generate the process counter metrics.
SELECT RUN_METRIC('android_mem_proc_counters.sql',
                  'table_name',
                  'file_rss',
                  'counter_names',
                  '("mem.rss.anon")');
SELECT RUN_METRIC('android_mem_proc_counters.sql',
                  'table_name',
                  'anon_rss',
                  'counter_names',
                  '("mem.rss.anon")');

CREATE VIEW Output AS
SELECT
  TraceMetrics(
    "android_mem",
    AndroidMemoryMetric(
      "system_metrics",
      AndroidMemoryMetric_SystemMetrics(
        "lmks",
        AndroidMemoryMetric_LowMemoryKills(
          "total_count",
          lmks.count
        )
      )
    )
  )
FROM
  (SELECT COUNT(*) as count from lmk_by_score) as lmks;
