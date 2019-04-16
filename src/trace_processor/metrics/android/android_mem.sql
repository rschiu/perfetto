CREATE TABLE last_oom_adj(upid BIG INT PRIMARY KEY, ts BIG INT, score INT);

INSERT INTO last_oom_adj
SELECT upid, ts, score
FROM (
  SELECT ref AS upid,
          ts,
          CAST(value AS INT) AS score,
          row_number() OVER (PARTITION BY ref ORDER BY ts DESC) AS rank
  FROM counters
  WHERE name = 'oom_score_adj'
  AND ref_type = 'upid')
WHERE rank = 1;

CREATE VIEW lmk_events AS
SELECT ref AS upid
FROM instants
WHERE name = 'mem.lmk' AND ref_type = 'upid';

CREATE VIEW lmk_by_score AS
SELECT process.name, last_oom_adj.score
FROM lmk_events
LEFT JOIN process ON lmk_events.upid = process.upid
LEFT JOIN last_oom_adj ON lmk_events.upid = last_oom_adj.upid
ORDER BY lmk_events.upid;
