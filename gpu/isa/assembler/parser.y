%code requires {
    #include "gpu/isa/assembler/pico_gpu_binary.h"

    struct DstRegister {
        Isa::RegisterSelection reg;
        uint32_t mask;
    };

    struct FullySwizzledRegister {
        Isa::RegisterSelection reg;
        Isa::SwizzlePatternComponent x;
        Isa::SwizzlePatternComponent y;
        Isa::SwizzlePatternComponent z;
        Isa::SwizzlePatternComponent w;
    };

    struct ImmediateArgs {
        size_t count;
        int32_t args[4];

        std::vector<int32_t> toVector() const {
            return std::vector<int32_t>(args, args + count);
        }
    };
}


%union {
    uint32_t ui;
    int32_t i;
    float f;
    DstRegister dstReg;
    FullySwizzledRegister fullySwizzledReg;
    Isa::RegisterSelection reg;
    Isa::SwizzlePatternComponent swizzleComponent;
    ImmediateArgs immediateArgs;
}

%{
    #include <stdio.h>
    #include <initializer_list>

    // Communication with scanner
    int gpuasm_lex();
    void scannerSetParsedString(const char *str);
    void scannerUnsetParsetString();
    extern int gpuasm_lineno;

    // Error handling
    int yyerror(void *yylval, const char *s);
    #define YYABORT_WITH_ERROR(err) { yyerror(nullptr, err); YYABORT; }
    #define VALIDATE_BINARY()                                         \
        do {                                                          \
            if (outputBinary->hasError()) {                           \
                YYABORT_WITH_ERROR(outputBinary->getError().c_str()); \
            }                                                         \
        } while (false)

    // Internal parser definitions
    bool hasDuplicates(std::initializer_list<Isa::SwizzlePatternComponent> values);
    uint32_t constructMask(std::initializer_list<Isa::SwizzlePatternComponent> values);
    inline int32_t asint(float arg) { return reinterpret_cast<int32_t&>(arg); }
%}

// Tokens received from lexer
%token FINIT FADD FSUB FMUL FDIV FNEG FDOT FCROSS
%token IINIT IADD ISUB IMUL IDIV INEG
%token MOV SWIZZLE
%token HASH_INPUT HASH_OUTPUT HASH_VS HASH_FS DOT
%token <swizzleComponent> VEC_COMPONENT
%token <reg> REG
%token <i> NUMBER_INT
%token <f> NUMBER_FLOAT

// Custom types defined by this parser
%type <ui> REG_MASK
%type <dstReg> DST_REG DST_REG_COMPONENT
%type <fullySwizzledReg> FULLY_SWIZZLED_REG
%type <immediateArgs> IMMEDIATE_INTS IMMEDIATE_FLOATS

%parse-param {Isa::PicoGpuBinary *outputBinary}

%define api.prefix {gpuasm_}
%define parse.error verbose






%%





PROGRAM: DIRECTIVE_SECTION INSTRUCTION_SECTION

// ----------------------------- Directives
DIRECTIVE_SECTION : DIRECTIVES { outputBinary->finalizeDirectives(); VALIDATE_BINARY(); }
DIRECTIVES:
      DIRECTIVE
    | DIRECTIVES DIRECTIVE
DIRECTIVE : INPUT_DIRECTIVE | OUTPUT_DIRECTIVE | PROGRAM_TYPE_DIRECTIVE
INPUT_DIRECTIVE :  HASH_INPUT  REG REG_MASK  { outputBinary->encodeDirectiveInputOutput($2, $3, true);  VALIDATE_BINARY(); }
OUTPUT_DIRECTIVE : HASH_OUTPUT REG REG_MASK  { outputBinary->encodeDirectiveInputOutput($2, $3, false); VALIDATE_BINARY(); }
PROGRAM_TYPE_DIRECTIVE:
      HASH_VS { outputBinary->encodeDirectiveShaderType(Isa::Command::ProgramType::VertexShader); }
    | HASH_FS { outputBinary->encodeDirectiveShaderType(Isa::Command::ProgramType::FragmentShader); }



// ----------------------------- Instructions
INSTRUCTION_SECTION : INSTRUCTIONS { outputBinary->finalizeInstructions(); VALIDATE_BINARY(); }
INSTRUCTIONS:
      INSTRUCTION
    | INSTRUCTIONS INSTRUCTION

INSTRUCTION:
      FADD      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::fadd, $2.reg, $3, $4, $2.mask); }
    | FADD      DST_REG REG NUMBER_FLOAT  { outputBinary->encodeBinaryMathImm(Isa::Opcode::fadd_imm, $2.reg, $3, $2.mask, {asint($4)}); }
    | FSUB      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::fsub, $2.reg, $3, $4, $2.mask); }
    | FSUB      DST_REG REG NUMBER_FLOAT  { outputBinary->encodeBinaryMathImm(Isa::Opcode::fsub_imm, $2.reg, $3, $2.mask, {asint($4)}); }
    | FMUL      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::fmul, $2.reg, $3, $4, $2.mask); }
    | FMUL      DST_REG REG NUMBER_FLOAT  { outputBinary->encodeBinaryMathImm(Isa::Opcode::fmul_imm, $2.reg, $3, $2.mask, {asint($4)}); }
    | FDIV      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::fdiv, $2.reg, $3, $4, $2.mask); }
    | FDIV      DST_REG REG NUMBER_FLOAT  { outputBinary->encodeBinaryMathImm(Isa::Opcode::fdiv_imm, $2.reg, $3, $2.mask, {asint($4)}); }
    | FNEG      DST_REG REG               { outputBinary->encodeUnaryMath(Isa::Opcode::fneg, $2.reg, $3, $2.mask); }
    | FDOT      DST_REG_COMPONENT REG REG { outputBinary->encodeBinaryMath(Isa::Opcode::fdot, $2.reg, $3, $4, $2.mask); }
    | FCROSS    REG REG REG               { outputBinary->encodeBinaryMath(Isa::Opcode::fcross, $2, $3, $4, 0b1111); }
    | IADD      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::iadd, $2.reg, $3, $4, $2.mask); }
    | IADD      DST_REG REG NUMBER_INT    { outputBinary->encodeBinaryMathImm(Isa::Opcode::iadd_imm, $2.reg, $3, $2.mask, {$4}); }
    | ISUB      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::isub, $2.reg, $3, $4, $2.mask); }
    | ISUB      DST_REG REG NUMBER_INT    { outputBinary->encodeBinaryMathImm(Isa::Opcode::isub_imm, $2.reg, $3, $2.mask, {$4}); }
    | IMUL      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::imul, $2.reg, $3, $4, $2.mask); }
    | IMUL      DST_REG REG NUMBER_INT    { outputBinary->encodeBinaryMathImm(Isa::Opcode::imul_imm, $2.reg, $3, $2.mask, {$4}); }
    | IDIV      DST_REG REG REG           { outputBinary->encodeBinaryMath(Isa::Opcode::idiv, $2.reg, $3, $4, $2.mask); }
    | IDIV      DST_REG REG NUMBER_INT    { outputBinary->encodeBinaryMathImm(Isa::Opcode::idiv_imm, $2.reg, $3, $2.mask, {$4}); }
    | INEG      DST_REG REG               { outputBinary->encodeUnaryMath(Isa::Opcode::ineg, $2.reg, $3, $2.mask); }
    | SWIZZLE   REG FULLY_SWIZZLED_REG    { outputBinary->encodeSwizzle(Isa::Opcode::swizzle, $2, $3.reg, $3.x, $3.y, $3.z, $3.w); }
    | MOV       DST_REG REG               { outputBinary->encodeUnaryMath(Isa::Opcode::mov, $2.reg, $3, $2.mask); }
    | FINIT     DST_REG IMMEDIATE_FLOATS  { outputBinary->encodeUnaryMathImm(Isa::Opcode::init, $2.reg, $2.mask, $3.toVector()); }
    | IINIT     DST_REG IMMEDIATE_INTS    { outputBinary->encodeUnaryMathImm(Isa::Opcode::init, $2.reg, $2.mask, $3.toVector()); }

// ----------------------------- Miscealaneous constructs
DST_REG:
      REG          { $$ = DstRegister{$1, 0b1111}; }
    | REG REG_MASK { $$ = DstRegister{$1, $2}; }

DST_REG_COMPONENT:
    REG DOT VEC_COMPONENT { $$ = DstRegister{$1, constructMask({$3})}; }

REG_MASK:
      DOT VEC_COMPONENT                                           {                                                                                                       $$ = constructMask({$2});             }
    | DOT VEC_COMPONENT VEC_COMPONENT                             { if (hasDuplicates({$2, $3}))         YYABORT_WITH_ERROR("duplicate components in dst register mask"); $$ = constructMask({$2, $3});         }
    | DOT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT               { if (hasDuplicates({$2, $3, $4}))     YYABORT_WITH_ERROR("duplicate components in dst register mask"); $$ = constructMask({$2, $3, $4});     }
    | DOT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT { if (hasDuplicates({$2, $3, $4, $5})) YYABORT_WITH_ERROR("duplicate components in dst register mask"); $$ = constructMask({$2, $3, $4, $5}); }

FULLY_SWIZZLED_REG:
    REG DOT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT { $$ = FullySwizzledRegister{$1, $3, $4, $5, $6}; }

IMMEDIATE_INTS:
      NUMBER_INT                                   { $$ = ImmediateArgs{ 1, {$1,  0,  0,  0}}; }
    | NUMBER_INT NUMBER_INT                        { $$ = ImmediateArgs{ 2, {$1, $2,  0,  0}}; }
    | NUMBER_INT NUMBER_INT NUMBER_INT             { $$ = ImmediateArgs{ 3, {$1, $2, $3,  0}}; }
    | NUMBER_INT NUMBER_INT NUMBER_INT NUMBER_INT  { $$ = ImmediateArgs{ 4, {$1, $2, $3, $4}}; }

IMMEDIATE_FLOATS:
      NUMBER_FLOAT                                         { $$ = ImmediateArgs{ 1, {asint($1),         0,         0,         0}}; }
    | NUMBER_FLOAT NUMBER_FLOAT                            { $$ = ImmediateArgs{ 2, {asint($1), asint($2),         0,         0}}; }
    | NUMBER_FLOAT NUMBER_FLOAT NUMBER_FLOAT               { $$ = ImmediateArgs{ 3, {asint($1), asint($2), asint($3),         0}}; }
    | NUMBER_FLOAT NUMBER_FLOAT NUMBER_FLOAT NUMBER_FLOAT  { $$ = ImmediateArgs{ 4, {asint($1), asint($2), asint($3), asint($4)}}; }



%%





int gpuasm_error(void *yylval, const char *s)
{
    fprintf(stderr, "error: %s (line: %d)\n", s, gpuasm_lineno);
    return 0;
}

bool hasDuplicates(std::initializer_list<Isa::SwizzlePatternComponent> values) {
    for (auto p1 = values.begin(); p1 != values.end(); p1++) {
        for (auto p2 = p1 + 1; p2 != values.end(); p2++) {
            if (*p1 == *p2) {
                return true;
            }
        }
    }
    return false;
}

uint32_t constructMask(std::initializer_list<Isa::SwizzlePatternComponent> values) {
    uint32_t mask = 0;
    for (auto p1 = values.begin(); p1 != values.end(); p1++) {
        uint32_t component = static_cast<uint32_t>(*p1);
        uint32_t bit = 1 << (3 - component);
        mask |= bit;
    }
    return mask;
}

namespace Isa {
    int assembly(const char *code, PicoGpuBinary *outBinary)  {
        scannerSetParsedString(code);
        int result = gpuasm_parse(outBinary);
        scannerUnsetParsetString();
        return result;
    }
}
