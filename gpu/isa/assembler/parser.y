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
}


%union {
    uint32_t ui;
    DstRegister dstReg;
    FullySwizzledRegister fullySwizzledReg;
    Isa::RegisterSelection reg;
    Isa::SwizzlePatternComponent swizzleComponent;
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

    // Internal parser definitions
    bool hasDuplicates(std::initializer_list<Isa::SwizzlePatternComponent> values);
    uint32_t constructMask(std::initializer_list<Isa::SwizzlePatternComponent> values);
%}

%token ADD MOV SWIZZLE
%token <swizzleComponent> VEC_COMPONENT
%token HASH_INPUT HASH_OUTPUT
%token DOT
%token <reg> REG_GENERAL REG_INPUT REG_OUTPUT

%type <ui> REG_MASK
%type <dstReg> DST_REG
%type <fullySwizzledReg> FULLY_SWIZZLED_REG
%type <reg> REG

%parse-param {Isa::PicoGpuBinary *outputBinary}

%define api.prefix {gpuasm_}
%define parse.error verbose






%%





PROGRAM: DIRECTIVE_SECTION INSTRUCTION_SECTION

// ----------------------------- Directives
DIRECTIVE_SECTION : DIRECTIVES { const char *error; if(!outputBinary->finalizeDirectives(&error)) { YYABORT_WITH_ERROR(error); } }
DIRECTIVES:
      DIRECTIVE
    | DIRECTIVES DIRECTIVE
DIRECTIVE : INPUT_DIRECTIVE | OUTPUT_DIRECTIVE
INPUT_DIRECTIVE :  HASH_INPUT  REG_INPUT REG_MASK  { outputBinary->encodeDirectiveInput($3); }
OUTPUT_DIRECTIVE : HASH_OUTPUT REG_OUTPUT REG_MASK { outputBinary->encodeDirectiveOutput($3); }



// ----------------------------- Instructions
INSTRUCTION_SECTION : INSTRUCTIONS { const char *error; if(!outputBinary->finalizeInstructions(&error)) { YYABORT_WITH_ERROR(error); } }
INSTRUCTIONS:
      INSTRUCTION
    | INSTRUCTIONS INSTRUCTION

INSTRUCTION:
      ADD     DST_REG REG REG        { outputBinary->encodeBinaryMath(Isa::Opcode::add, $2.reg, $3, $4, $2.mask); }
    | MOV     DST_REG REG            { outputBinary->encodeUnaryMath(Isa::Opcode::mov, $2.reg, $3, $2.mask); }
    | SWIZZLE REG FULLY_SWIZZLED_REG { outputBinary->encodeSwizzle(Isa::Opcode::swizzle, $2, $3.reg, $3.x, $3.y, $3.z, $3.w); }


// ----------------------------- Miscealaneous constructs
DST_REG:
      REG          { $$ = DstRegister{$1, 0b1111}; }
    | REG REG_MASK { $$ = DstRegister{$1, $2}; }

REG:
      REG_GENERAL
    | REG_INPUT
    | REG_OUTPUT

REG_MASK:
      DOT VEC_COMPONENT                                           {                                                                                                       $$ = constructMask({$2});             }
    | DOT VEC_COMPONENT VEC_COMPONENT                             { if (hasDuplicates({$2, $3}))         YYABORT_WITH_ERROR("duplicate components in dst register mask"); $$ = constructMask({$2, $3});         }
    | DOT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT               { if (hasDuplicates({$2, $3, $4}))     YYABORT_WITH_ERROR("duplicate components in dst register mask"); $$ = constructMask({$2, $3, $4});     }
    | DOT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT { if (hasDuplicates({$2, $3, $4, $5})) YYABORT_WITH_ERROR("duplicate components in dst register mask"); $$ = constructMask({$2, $3, $4, $5}); }

FULLY_SWIZZLED_REG:
    REG DOT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT VEC_COMPONENT { $$ = FullySwizzledRegister{$1, $3, $4, $5, $6}; } 




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
