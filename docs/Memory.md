# Memory in PicoGpu

## Overview
*PicoGpu* cannot use *host-side* memory, i.e. memory created on the heap or stack in `sc_main`. All its internal blocks use the *gpu-side* memory contained in the device. A simple single channel read-write memory is implemented by storing multiple `sc_signal` members inside the `Memory` module. Client of the memory can use its input pins to request read or write operations on a single dword. The memory signals completion of the requested operation by its output pin. The client should always check the completion output pin and should not make any assumptions on the request latency. All addresses passed to the memory should be divisible by 4 - it is undefined to perform operations crossing the dword boundary. The number of dwords stored in memory is statically defined by a template argument. The `Gpu` module implicitly sets its main memory size to 21000 dwords.



## Multiple clients
Because the memory can only serve one client, additional logic is needed to connect multiple blocks to it. The `MemoryController` module connects to the `Memory` as its only client, but allows having multiple clients itself. The number of allowed clients of the controller is statically defined by a template argument set to a required value by the `Gpu` module. Multiple blocks requiring memory access, such as `VertexShader` or `OutputMerger` connect to the `MemoryController`, which arbitrates their memory requests. It selects one request at a time, forwards it to the actual memory and signals completion to the client, which originally made the request. Hence, access time from the perspective of a client block may vary depending on how many other blocks are making requests.



## Memory transfers
In order for user-specified data to be used by the *PicoGpu* it must be transfered from *host-side* memory to *gpu-side* memory. Whenever the user needs to inspect the rendering results (e.g. to display them), *gpu-side* framebuffer storage must be copied to a *host-side* buffer. These operations can only by performed by a `Blitter` block. The `Blitter` can copy a number of dwords in both directions between host and device. It can also execute a fill operation, i.e. write the same value in multiple locations in *gpu-side* memory. The user can execute blit calls through the `CommandStreamer` interface (see [documentation](/docs/TaskLaunching.md)).




## Other uses
It is of course possible to connect a GPU block directly to a `Memory` module, but this module would only be usable by that one block. This could be used as a way for the block to implement a caching mechanism, but is not currently used by *PicoGpu*.

Because `MemoryController` module has the same interface for clients as `Memory`, it is possible to chain them, i.e. connect a `MemoryController` as a client of another `MemoryController` to form a more sophisticated memory hierarchy. While possible, this mechanism is not currently used by *PicoGpu*.
