# Shader Programming

*PicoGpu* is equipped with shading units capable of performing mathematical operations, branching and register transfers. Each shading unit executes in SIMD32 mode, meaning each cycle it will execute an instruction for up to 32 different threads. Program counter is shared between all threads in a shader unit.

The program binary and its inputs are passed as a data stream to the shading unit. The assembler (see [gpu/isa/assembler](gpu/isa/assembler)) can parse a *PicoGpu* assembly and convert it into data stream ready to be sent to the shading unit.

# Registers
All instructions utilize the following registers:

| Register type         | Symbol      | Size                       | Count              | Description |
|-----------------------|-------------|----------------------------|--------------------|-------------|
| Input                 | i0, i1, ... | 4 components, 32 bits each | 4 per thread       | Can be initialized with external data from pipeline, e.g. vertex positions. Other than that, they can be used as a GPR |
| Output                | o0, o1, ... | 4 components, 32 bits each | 4 per thread       | Can be sent back to the caller after executing the program. Other than  that, they can be used as a GPR |
| General purpose (GPR) | r0, r1, ... | 4 components, 32 bits each | 8 per thread       | Can be used for any operation |
| Program counter (PC)  | N/A         | 32 bits                    | 1 per shading unit | Defines current position within the instruction buffer for all threads. It is advanced automatically and can be manipulated through jump instructions |

In the following sections the following placeholders will be used to denote the registers:
- {ireg} - any input register
- {oreg} - any output register
- {r} - any register (input, output or general purpose)
- {reg/int} - any register or an integer immediate value
- {reg/float} - any register or a floating point immediate value
- {mask} - a combination of x,y,z,w components. Each component can be used only once
- {int} - integer constant
- {float} - floating point constant

# Directives
The prologue of *PicoGpu* assembly is a number of directives. All directives start with a `#` sign.

| Directive             | Description |
|-----------------------|-------------|
| #input {ireg}.{mask}  | Defines an input register to be initialized. Only components contained in the mask will be initialized,  the rest will be 0. At least one input is required. |
| #output {oreg}.{mask} | Defines an output register to be sent back to the caller. Only components contained in the mask will be sent back. At least one output is required. |


# Instructions
The shader unit can interpret and execute multiple instructions. All instructions must come after directives. They cannot be interleaved with each other. The first register argument is always a destination register in all math-related instructions.

## Integer math
| Instruction | Description |
|-|-|
| iadd {reg}.{mask} {reg} {reg/int} | Adds two values      |
| isub {reg}.{mask} {reg} {reg/int} | Subtracts two values |
| imul {reg}.{mask} {reg} {reg/int} | Multiply two values  |
| idiv {reg}.{mask} {reg} {reg/int} | Divides two values   |
| ineg {reg}.{mask} {reg}           | Negates a value      |


# Floating point math
| Instruction | Description |
|-------------|-------------|
| fadd {reg}.{mask} {reg} {reg/float} | Adds two values        |
| fsub {reg}.{mask} {reg} {reg/float} | Subtracts two values   |
| fmul {reg}.{mask} {reg} {reg/float} | Multiply two values    |
| fdiv {reg}.{mask} {reg} {reg/float} | Divides two values     |
| fdot {reg} {reg} {reg}              | Calculates a dot product and stores result in x component of the first register |

# Control logic
| Instruction | Description |
|-------------|-------------|
| jmp {int}   | Set program counter to a given offset within instruction buffer |