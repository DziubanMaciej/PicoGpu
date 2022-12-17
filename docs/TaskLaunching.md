# Task Launching

All *host-side* user interaction with operations performed by *PicoGpu* (except for [pin configuration](/docs/ConfigurationPins.md)) is handled by the `CommandStreamer` block. It exposes a number of *host-side* methods, such as `draw()` or `blitToMemory()` and **asynchronously** schedules requested commands to the GPU, whenever it's ready. This design greatly simplifies user interaction with the devices and does not require to be aware which blocks will actually execute the work.

Although the graphics pipeline and memory blitter can technically work in parallel, `CommandStreamer` does not allow this and performs a full stall before all requests. Hence, the user does not have to wait for the commands to complete before issuing more of them - the order of operations is guaranteed.

The `CommandStreamer` also exposes `waitForIdle()` method to stall *client-side* code until the GPU is done with all scheduled computations. This would typically be used after scheduling a blit from memory to read the rendered framebuffer before displaying it to the screen.
