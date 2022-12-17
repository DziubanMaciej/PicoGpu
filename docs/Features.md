# Existing features

| Feature                                      | Comment                                                                                                   |
| -------------------------------------------- | --------------------------------------------------------------------------------------------------------- |
| Rendering multiple triangles in one drawcall | **PA** iterates over triangles.                                                                           |
| VCD traces                                   | Traces are dumped in the binary directory.                                                                |
| Multi-client memory                          | **MEMCTL** arbitrates access of clients to memory.                                                        |
| Copying between host memory and GPU memory   | **BLT** performs memory transfers.                                                                        |
| Depth testing                                | **OM** performs depth test.                                                                               |
| Floating point data                          | Data flowing through 32-bit wide ports are assumed to be floating point by various blocks.                |
| Programmability                              | **SU** can execute *PicoGpu* ISA.                                                                         |
| Vertex shader                                | Launches threads via **SF**.                                                                              |
| Fragment shader                              | Launches threads via **SF**.                                                                              |
| Signals for profiling                        | All blocks have their own signal indicating, whether they are doing any work.                             |
| Unified frontend for launching tasks         | **CS** is the only block, which the host has to interact with.                                            |
| Barycentric coordinates calculation          | Special code is injected at the beginning of fragment shaders to calculate weights.                       |
| Uniform values                               | Shaders can define uniform register, which will be initialized to values set in pipeline state registers. |

# Features to implement

| Feature                           | Comment                                                                                                                           |
| --------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| Optimize data passing             | Some blocks could use parallel ports for faster data passing.                                                                     |
| Better rasterization algorithm    | **RS** blindly iterates over every pixel.                                                                                         |
| Connect shader units to memory    | Add a separate MemoryController just for the shader units?                                                                        |
| Add matrix operations to the ISA  | Will have to use 4 subsequent register as a 4x4 matrix. Complicated range checking. Takes 12 out of 16 register to do anything... |
| Implement conditions in ISA       | Gets really complicated to handle thread divergence and operation masking.                                                        |
