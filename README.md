# PicoGpu
This project as an implementation of a simplified GPU using [SystemC](https://systemc.org/) environment. The GPU has its own memory allowing multiple clients to utilize it. The rendering process is broken down into multiple hardware blocks performing specialized tasks and optionally using the memory. Device is not programmable yet, it only allows some configuration.

# Architecture


# Features
The project is not very mature and it lacks many features. Existing functionalities as well as planned future improvements are presented in the table below
|Feature|Status|
|------|---|
|Multi-client memory| :heavy_check_mark: `MemoryController` arbitrates access of clients to memory|
|Copying between system memory and GPU memory |  :heavy_check_mark: Implemented `UserBlitter` |
|Depth test| :heavy_check_mark: `OutputMerger` performs depth test |
| Programmability | :x: Some unified frontend to scheduling threads by multiple clients (i.e. vertex/fragment shader blocks) will be needed. |
| Vertex shader | :x: |
| Fragment shader | :x: |

