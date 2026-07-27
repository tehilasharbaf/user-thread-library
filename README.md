##### User-Level Thread Library (`uthreads`)



This project implements a **user-level threading library** in C++. It features a custom round-robin scheduler that utilizes POSIX virtual timers (`SIGVTALRM`) to preemptively manage thread execution in user space, operating entirely independent of OS-level kernel threads.



###### Core Features



\* Round-Robin Scheduling: Automatically preempts and cycles through threads based on a configurable time quantum.

\* State Management: Transitions threads seamlessly between `RUNNING`, `READY`, and `BLOCKED` states.

\* Virtual Timers: Utilizes `ITIMER\_VIRTUAL` and signal handling to trigger deterministic context switches.

\* Custom Context Switching: Leverages `sigsetjmp` and `siglongjmp` alongside x86\_64 inline assembly for stack pointer manipulation.

\* Thread Sleeping: Allows threads to voluntarily block for a specified number of scheduling quantums without halting the main process.



###### File Structure



\* `uthreads.h`: The external API definition containing the function signatures available to the user.

\* `uthreads.cpp`: The implementation of the external API, bridging user commands to the internal scheduling system.

\* `threads.h`: Internal class definitions for the `CThread`, `Spawner`, and `Scheduler` objects.

\* `threads.cpp`: Core implementation of thread state management, dynamic memory allocation, and the scheduling algorithm.



###### System Requirements



This library relies strictly on POSIX system calls, signal masking, and x86\_64 architecture-specific instructions. It is not compatible with native Windows compilation.



\* Operating System: Linux (e.g., Ubuntu via WSL, or a remote Linux host).

\* Compiler: `g++` (or equivalent C++ compiler supporting C++11 or higher).

\* Architecture: x86\_64 (required for the virtual-to-physical address translation assembly).



###### Compilation



```bash

g++ -Wall -Wextra -std=c++11 uthreads.cpp threads.cpp main.cpp -o uthreads\_app



```



###### API Overview



Include `"uthreads.h"` in your application to interface with the library. The main thread (TID 0) is automatically initialized as the first running thread.



\* `uthread\_init(int quantum\_usecs)`: Initializes the library and sets the time slice for the round-robin scheduler. Must be called before any other function.

\* `uthread\_spawn(thread\_entry\_point entry\_point)`: Creates a new thread, allocates a 4KB stack, and adds it to the back of the ready queue.

\* `uthread\_terminate(int tid)`: Terminates a specific thread and safely reclaims its memory during the next context switch.

\* `uthread\_block(int tid)`: Suspends the execution of a specific thread, moving it to the `BLOCKED` state.

\* `uthread\_resume(int tid)`: Returns a blocked thread to the `READY` queue.

\* `uthread\_sleep(int num\_quantums)`: Blocks the currently running thread for a set number of timer ticks.

\* `uthread\_get\_tid()`: Returns the ID of the currently executing thread.

\* `uthread\_get\_total\_quantums()`: Returns the total number of scheduling ticks since the library was initialized.

\* `uthread\_get\_quantums(int tid)`: Returns the total number of quantums a specific thread has spent actively `RUNNING`.

