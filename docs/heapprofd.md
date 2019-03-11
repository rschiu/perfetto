# heapprofd

Low-overhead native heap profiling mechanism, with C++ and Java callstack
attribution, usable by all processes on an Android system.

* Out-of-process unwinding for significantly reduced overhead on the profiled
  process and increased stability.
* Variable sample rate, choose a sample rate in bytes to trade off accuracy and
  overhead.
* Attach to already running processes

For usage instructions see the
[heapprofd README](../src/profiling/memory/README).
