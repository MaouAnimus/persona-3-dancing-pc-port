# Event flag runtime correction

Date: 2026-10-17 (America/Santiago)

The event flag implementation in `tools/gow3_ref/src/runtime_kernel.c` had three observed contract errors: OR waits consumed their requested bits without a clear flag, CLEAR_ALL/CLEAR_PAT were ignored, and finite waits ignored their timeout. Creation also wrote a 32-bit value to the 64-bit event handle output. Deletion destroyed synchronization primitives while waiters could still reference them.

The replacement uses pointer-sized monotonically allocated opaque handles, a locked waiter registry, FIFO matching and consumption, independent AND/OR and clear modes, 32-bit microsecond deadlines, and deletion wakeups with EACCES. SINGLE/MULTI attributes are enforced. Priority queues are approximated by FIFO, consistently with the existing semaphore runtime.

Source baseline: `runtime_kernel.before-eventflags.c`, SHA-256 `85B8DBA3685141A068DF409AB48870DE87E818F08DB26037780217ECD7184828`. Source text before and after the replaced event flag section was compared with this exact baseline and matched, preserving unrelated concurrent edits.

Primary API references:

- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/docs/MD/PS4%20Libraries/Libkernel.md
- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/libkernel.h
- https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/kernel/threads/event_flag.cpp

Regression test: `tools/gow3_ref/tests/test_eventflag.c`.

Built from `tools/gow3_ref` with the existing MSYS2 CLANG64 compiler:

```
C:\msys64\clang64\bin\clang.exe -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_eventflag.c -o out/eventflag-test.exe
```

Executed with `C:\msys64\clang64\bin` in PATH, exit status 0:

```
event flag tests: ok (ABI, AND/OR, clearing, timeout, FIFO, deletion, SINGLE/MULTI)
```

The test checks handle width and an adjacent canary, complete pre-clear output patterns, AND versus OR, both clear modes, zero and 20 ms timeouts, the 32-bit timeout output with a canary, two queued consuming waiters, deletion of two blocked waiters, stale handle rejection, SINGLE/MULTI behavior, and invalid argument validation.

This verifies runtime contracts. It does not establish that these bugs caused the original game's current black screen or that gameplay now works. The existing Antigravity game process was neither stopped nor altered for this validation. No full game launch or full runtime build was performed by this worker.
