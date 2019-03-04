create table tid_args as select arg_set_id, int_value as tid from args where key = "pid"

create index tid_args_idx on tid_args (arg_set_id)

create table prev_pid_args as select arg_set_id, int_value as tid from args where key = "prev_pid"

create index prev_pid_args_idx on prev_pid_args (arg_set_id)

create table next_pid_args as select arg_set_id, int_value as tid from args where key = "next_pid"

create index next_pid_args_idx on next_pid_args (arg_set_id)

create table prev_state_args as select arg_set_id, int_value as state from args where key = "prev_state"

create index prev_state_args_idx on prev_state_args (arg_set_id)

create table sched_wakeup as select ts, name, tid, -1 as state from raw join tid_args using(arg_set_id)

create table sched_end as select ts, "sched_end" as name, tid, state from raw join prev_pid_args using(arg_set_id) join prev_state_args using(arg_set_id)

create table sched_next as select ts, "sched_start" as name, tid, 0 as state from raw join next_pid_args using(arg_set_id)

create table thread_state as select * from sched_next union all select * from sched_end union all select * from sched_wakeup order by ts;

select (ts - (select start_ts from trace_bounds)) / 1e9 as ts, tid, name, state from thread_state where tid = 1027 limit 40
