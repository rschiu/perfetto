/* Create the file RSS table. */
CREATE VIEW file_rss AS
SELECT ts,
       LEAD(ts, 1, ts) OVER(PARTITION BY ref ORDER BY ts) - ts as dur,
       ref AS upid,
       value AS file
FROM counters
WHERE ref_type = 'upid' AND counters.name IN ("mem.rss.file", "rss_stat.mm_filepages");

/* Create the anon RSS table. */
create view anon_rss as
select ts,
       lead(ts, 1, ts) over(PARTITION by ref order by ts) - ts as dur,
       ref as upid,
       value as anon
from counters
where ref_type = 'upid' and counters.name in ("mem.rss.anon", "rss_stat.mm_anonpages");

/* Create the oom adj table. */
create view oom_adj as
select ts,
       lead(ts, 1, ts) over(PARTITION by ref order by ts) - ts as dur,
       ref as upid,
       value as oom_score_adj
from counters
where ref_type = 'upid' and counters.name = 'oom_score_adj';

/* Harmonise the three tables above into a single table. */
CREATE VIRTUAL TABLE anon_file USING span_join(anon_rss PARTITIONED upid, file_rss PARTITIONED upid);

CREATE VIRTUAL TABLE anon_file_oom USING span_join(anon_file PARTITIONED upid, oom_adj PARTITIONED upid);

/* For each span, compute the RSS (for a given upid) for each category of oom_adj */
CREATE VIEW rss_spans_for_oom_upid AS
SELECT ts,
       dur,
       upid,
       CASE WHEN oom_score_adj < 0 THEN file + anon ELSE 0 END as rss_oom_lt_zero,
       CASE WHEN oom_score_adj <= 0 THEN file + anon ELSE 0 END as rss_oom_eq_zero,
       CASE WHEN oom_score_adj < 20 THEN file + anon ELSE 0 END as rss_fg,
       CASE WHEN oom_score_adj < 700 THEN file + anon ELSE 0 END as rss_bg,
       CASE WHEN oom_score_adj < 900 THEN file + anon ELSE 0 END as rss_low_bg,
       file + anon as rss_cached
FROM anon_file_oom;

/* Convert the raw RSS values to the change in RSS (for a given upid) over time */
CREATE VIEW rss_spans_rss_change AS
SELECT ts,
       dur,
       upid,
       rss_oom_lt_zero - lag(rss_oom_lt_zero, 1, 0) OVER win as rss_oom_lt_zero_diff,
       rss_oom_eq_zero - lag(rss_oom_eq_zero, 1, 0) OVER win as rss_oom_eq_zero_diff,
       rss_fg - lag(rss_fg, 1, 0) OVER win as rss_fg_diff,
       rss_bg - lag(rss_bg, 1, 0) OVER win as rss_bg_diff,
       rss_low_bg - lag(rss_low_bg, 1, 0) OVER win as rss_low_bg_diff,
       rss_cached - lag(rss_cached, 1, 0) OVER win as rss_cached_diff
FROM rss_spans_for_oom_upid
WINDOW win AS (PARTITION BY upid ORDER BY ts);

/*
 * Compute a rolling sum of anon + file for each category of process state.
 * (note: we no longer consider upid in the windows which means we are now
 * computing a rolling sum for all the processes).
 */
CREATE VIEW output AS
SELECT ts,
       lead(ts, 1, ts) over win - ts as dur,
       SUM(rss_oom_lt_zero_diff) OVER win as rss_oom_lt_zero,
       SUM(rss_oom_eq_zero_diff) OVER win as rss_oom_eq_zero,
       SUM(rss_fg_diff) OVER win as rss_fg,
       SUM(rss_bg_diff) OVER win as rss_bg,
       SUM(rss_low_bg_diff) OVER win as rss_low_bg,
       SUM(rss_cached_diff) OVER win as rss_cached
FROM rss_spans_rss_change
WINDOW win as (ORDER BY ts)
ORDER BY ts;

/*
 * Print out the final result (note: dur = 0 is excluded to account for times
 * where multiple processes report a new memory value at the same timestamp)
 */
SELECT *
FROM output
WHERE dur > 0
