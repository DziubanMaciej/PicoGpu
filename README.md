# PicoGpu
This project is an implementation of a simple GPU (*graphics processing unit*) using [SystemC](https://systemc.org/) environment. The GPU has its own memory allowing multiple clients to utilize it. The rendering process is broken down into multiple hardware blocks performing specialized tasks and optionally using the memory. Device can be configured through multiple *SystemC* signals. It also contains programmable stages, which can be used to implement any graphics algorithm on the GPU side.

# Architecture
*PicoGpu* instantiates all its internal blocks, defines signals and connects all the blocks together. Internal blocks include:

- [Memory](gpu/blocks/memory.h) (**MEM**) and [MemoryController](gpu/blocks/memory_controller.h) (**MEMCTL**) - provide storage for various data required by the GPU, such as a vertex buffer or a frame buffer. *MemoryController* is a frontend to *Memory* allowing multiple clients (i.e. blocks, like **PA** and **OM**) to access it in a safe manner. All other blocks in *PicoGpu* have to communicate with *Memory* through the *MemoryController*. There is no direct connection.
- [ShaderUnit](gpu/blocks/shader_array/shader_unit.h) (**SU**) and [ShaderFrontend](gpu/blocks/shader_array/shader_frontend.h) (**SF**) - provide a means to execute *PicoGpu* shaders for programmable blocks in the graphics pipeline. Blocks only connect to the *ShaderFrontend* which serves as an arbiter and distributes work to individual *ShaderUnits* for execution.
- [User blitter](gpu/blocks/user_blitter.h) (**BLT**) - allows communicating between the *PicoGpu* and regular system memory (outside the simulation).
- Graphics pipeline:
  - [PrimitiveAssembler](gpu/blocks/primitive_assembler.h) (**PA**) - reads vertex data from specified memory location and streams it to the next block in groups of 9 (three vertices with x,y,z components).
  - [VertexShader](gpu/blocks/vertex_shader.h) (**VS**) - schedules a programmable shader for execution to the **ShaderFrontend**. The shader receives vertex position and has to output transformed vertex position.
  - [Rasterizer](gpu/blocks/rasterizer.h) (**RS**) - iterates over all pixels in framebuffer and checks if they are inside triangles streamed from previous block. Pixels that are inside, are then sent to the next block along with their color. Also performs perspective division.
  - [FragmentShader](gpu/blocks/fragment_shader.h) (**FS**) - schedules a programmable shader for execution to the **ShaderFrontend**. The shader receives interpolated vertex position and has to output 4-component RGBA color of a given pixel.
  - [Output Merger](gpu/blocks/output_merger.h) (**OM**) - writes color data to the framebuffer. Optionally performs the depth test.


![Architecture diagram](img/architecture.png)


# Repository structure
- [gpu](gpu) - static library containing all GPU functionality.
  - [blocks](gpu/blocks) - hardware blocks of the gpu.
  - [util](gpu/util) - utility functions not strictly connected with the *PicoGpu* project.
- [gpu_tests](gpu_tests) - source code for executable tests of the Gpu library. Due to how SystemC works, each test is contained in a separate executable.
- [third_party](third_party) - dependencies of the *PicoGpu* project

# Features
The project is not very mature and it lacks many features. Existing functionalities as well as planned future improvements are presented in the table below
| Feature                                      | Status                                                                                      |
|----------------------------------------------|---------------------------------------------------------------------------------------------|
| Render a triangle                            | :heavy_check_mark:                                                                          |
| Create vcd trace of all signals              | :heavy_check_mark:                                                                          |
| Multi-client memory                          | :heavy_check_mark: `MemoryController` arbitrates access of clients to memory                |
| Read vertex data from memory                 | :heavy_check_mark:                                                                          |
| Render multiple triangles                    | :heavy_check_mark:                                                                          |
| Copying between system memory and GPU memory | :heavy_check_mark: Implemented `UserBlitter`                                                |
| Depth test                                   | :heavy_check_mark: `OutputMerger` performs depth test                                       |
| Floating point data                          | :heavy_check_mark: Data flowing through 32-bit wide ports are assumed to be floating point by various blocks. |
| Programmability                              | :heavy_check_mark: `ShaderUnit` can execute our own *PicoGpu* ISA.                          |
| Vertex shader                                | :heavy_check_mark: A programmable stage before rasterization. Can alter vertex data         |
| Signals for profiling                        | :heavy_check_mark:                                                                          |
| Signals indicating business and completion   | :heavy_check_mark:                                                                          |
| Customizable vertex layout                   | :x: Currently only 3-component vertices can be passed.                                      |
| Fragment shader                              | :heavy_check_mark:                                                                          |
| Passing uniform data to shaders              | :x:                                                                                         |
| Perspective division in rasterizer           | :heavy_check_mark:                                                                          |
| Better waiting on completion                 | :x:                                                                                         |
| Moving additional data from VS to FS         | :x: Rasterizer will have to be aware of it somehow...                                       |
| Add a real-time visualization                | :x: Currently we dump the framebuffer to a png file                                         |


# Building and running
Requirements: Linux OS, SystemC environment, CMake and a C++ compiler.

Run all gpu tests:
```
mkdir -p build
cd build
cmake ..
make -j$(nproc)
ctest
```

Run *GpuTest* (tests the entire GPU instead of individual blocks)
```
./run_gpu.sh
```
