# Shader Programming
*PicoGpu* is equipped with shading units capable of performing mathematical operations and register transfers. Each shading unit executes in SIMD32 mode, meaning each cycle it will execute an instruction for up to 32 different threads. Program counter is shared between all threads in a shader unit. Fewer threads than 32 can be scheduled for execution, in which case remaining threads will be inactive.

The assembler (see [gpu/isa/assembler](gpu/isa/assembler)) can parse a *PicoGpu* assembly and convert it into data stream ready to be sent to the shading unit. The program binary and its inputs are passed as a data stream to the shading unit. After executing the binary, the shading unit streams all outputs back to the caller.

Each shader consists of two sections - directives and instructions. Directives serve as metadata of the shader program, describing how it interacts with the rest of *PicoGpu*. Instructions are what actually gets executed by shader units. Instructions can alter any of the registers exposed to the shader programmer. Note that overwriting values of input registers is forbidden and can yield undefined results.



# Registers
All instructions can utilize the following registers:

| Register type         | Symbol      | Size                       | Count              | Description                                                                                                                             |
| --------------------- | ----------- | -------------------------- | ------------------ | --------------------------------------------------------------------------------------------------------------------------------------- |
| General purpose (GPR) | r0, r1, ... | 4 components, 32 bits each | 16 per thread      | Can be used for any operation. Can also be defined as an input, output or uniform.                                                      |
| Program counter (PC)  | N/A         | 32 bits                    | 1 per shading unit | Defines current position within the instruction buffer for all threads. It is advanced automatically and cannot be manipulated directly |

The following symbols will be used to describe parameters to directives and instructions:
- {reg} - any general purpose register
- {mask} - a combination of 1-4 components x,y,z or w. Each component can be used only once,
- {iomask} - a combination of 1-4 components x,y,z or w. Each component can be used only once. Components must be used in order, e.g. `xy` or `xyz`, but not `xyw`.
- {srcmask} - a combination of 4 components x,y,z or w. Each component can be used multiple times,
- {int} - integer constant,
- {float} - floating point constant,
- [k...n] - specifies allowed quantity of preceding token to be between `k` and `n`. For example: `{int}[1..4]` means 1,2,3 or 4 integer immediate values.



# Directives
The prologue of *PicoGpu* assembly is a number of directives. All directives start with a `#` sign.

| Directive               | Description                                                                                                    |
| ----------------------- | -------------------------------------------------------------------------------------------------------------- |
| #input {reg}.{iomask}   | Defines an input register.                                                                                     |
| #output {reg}.{iomask}  | Defines an output register.                                                                                    |
| #uniform {reg}.{iomask} | Defines a uniform register.                                                                                    |
| #vertexShader           | Sets type of current shader as vertex shader. There must be only one shader type directive.                    |
| #fragmentShader         | Sets type of current shader as fragment shader. There must be only one shader type directive.                  |
| #undefinedRegs          | Allows unused registers to have undefined values instead of zero-initializing them. Can reduce launch latency. |

Input registers are initialized with their thread-specific values at the beginning of the shader execution. Only components contained in `iomask` of the input directive are set to input values, the rest are zero-initialized. Origin of these values depends on the [shader type](#Shader-types). A single register cannot be used in multiple input directives as well as both as input and a uniform.

Output registers are sent back to the GPU pipeline for further processing. Only components contained in `iomask` of the output directive are sent back, the rest are ignored. There may be different requirements on outputs depending on the [shader type](#Shader-types). A single register cannot be used in multiple output directives. However, it can be used both as an input and output to implement a passthrough effect.

Uniform registers are initialized with values set in the GPU pipeline state. The values are the same for all threads. Components in `iomask` are set to values set in the pipeline state, the rest are zero-initialized. A single register cannot be used in multiple uniform directives as well as both as a uniform and input.



# Shader types
Every shader has to contain a shader type directive - either `#fragmentShader` or `#vertexShader`. The directive will set the shader type and allow it to be used only by designated hardware block - [VertexShader](gpu/blocks/vertex_shader.h) and [FragmentShader](gpu/blocks/fragment_shader.h) respectively. Shader type can also add some additional requirements and/or limitations to the programming model.

Vertex shaders must take between 1 and 3 input parameters, which will be taken from the vertex buffer. They must output between 1 and 3 parameters. First parameter has to be 4-component vector containing position. Remaining two are called custom attributes. They can be used to pass values like normals or tex coords to the fragment shader.

Fragment shaders must take between 1 and 3 input parameters, which must match output parameters produced by the vertex shader. First input must be a 4-component position vector. The z-value and the custom input attributes will be interpolated based on values at triangle vertices. Fragment shader must return only one vector with 4 components containing computed RGBA color. *PicoGpu* will internally append additional parameter containing interpolated z-value. Although this is completely hidden from the shader programmer.



# Instructions
The shader unit can interpret and execute multiple instructions. All instructions must come after all directives - they cannot be interleaved with each other. The first register argument is always a destination register in all math-related instructions. Destination register mask specifies the components on which the operation shall be performed. If it is omitted, an implicit `xyzw` mask is used.

Instructions with immediate arguments (`{int}` or `{float}`) all take between 1 and 4 values. If there are too few values for a given mask, the last specified value is duplicated. For example `iadd r0.xyz r0 1 2` is functionally equivalent to `iadd r0.xyz 1 2 2`.

## Integer math
| Instruction                                                           | Description                               |
|-----------------------------------------------------------------------|------------------------------------------ |
| iadd {reg}.{mask} {reg} {reg}</br>iadd {reg}.{mask} {reg} {int}[1..4] | Adds two values                           |
| isub {reg}.{mask} {reg} {reg}</br>isub {reg}.{mask} {reg} {int}[1..4] | Subtracts two values                      |
| imul {reg}.{mask} {reg} {reg}</br>imul {reg}.{mask} {reg} {int}[1..4] | Multiply two values                       |
| idiv {reg}.{mask} {reg} {reg}</br>idiv {reg}.{mask} {reg} {int}[1..4] | Divides two values                        |
| ineg {reg}.{mask} {reg}                                               | Negates a value                           |
| imax {reg}.{mask} {reg}                                               | Calculates a component-wise maximum value |
| imin {reg}.{mask} {reg}                                               | Calculates a component-wise minimum value |


## Floating point math
| Instruction                                                                | Description                                                                                                       |
| -------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| fadd    {reg}.{mask} {reg} {reg}</br>fadd {reg}.{mask} {reg} {float}[1..4] | Adds two values                                                                                                   |
| fsub    {reg}.{mask} {reg} {reg}</br>fsub {reg}.{mask} {reg} {float}[1..4] | Subtracts two values                                                                                              |
| fmul    {reg}.{mask} {reg} {reg}</br>fmul {reg}.{mask} {reg} {float}[1..4] | Multiply two values                                                                                               |
| fdiv    {reg}.{mask} {reg} {reg}</br>fdiv {reg}.{mask} {reg} {float}[1..4] | Divides two values                                                                                                |
| fneg    {reg}.{mask} {reg}                                                 | Negates a value                                                                                                   |
| fcross  {reg}.{mask} {reg} {reg}                                           | Calculates a 3-dimensional cross product and stores result in `xyz` components. A zero is stored in `w` component |
| fdot    {reg}.{mask} {reg} {reg}                                           | Calculates a 4-dimensional dot product and stores result in all components                                        |
| fcross2 {reg}.{mask} {reg} {reg}                                           | Calculates a 2-dimensional cross product being `Ax*By - Ay*Bx` and stores result in all components                |
| fmad    {reg}.{mask} {reg} {reg} {reg}                                     | Multiplies first two src values together and adds the third src value                                             |
| fnorm   {reg}.{mask} {reg}                                                 | Normalizes a vector                                                                                               |
| frcp    {reg}.{mask} {reg}                                                 | Calculates a component-wise reciprocal                                                                            |
| fmax    {reg}.{mask} {reg}                                                 | Calculates a component-wise maximum value                                                                         |
| fmin    {reg}.{mask} {reg}                                                 | Calculates a component-wise minimum value                                                                         |

## Miscellaneous
| Instruction                                                              | Description                                                             |
|--------------------------------------------------------------------------|------------------------------------------------------------------------ |
| finit {reg}.{mask} {float}[1..4]</br>iinit {reg}.{mask} {int}[1..4]</br> | Loads immediate values into a register                                  |
| swizzle {reg} {reg}.{srcmask}                                            | Swizzles components of src register and stores it into dst register     |
| mov {reg}.{mask} {reg}                                                   | Moves contents of one register to another                               |
| trap                                                                     | Triggers a debugger breakpoint. Does nothing if debugger is not present |
