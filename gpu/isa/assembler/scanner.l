%{
#include "inc.h"
#include <unistd.h>

static int processVecComponent(char c, GPUASM_STYPE &yyval);
static Isa::RegisterSelection calculateRegisterSelection(Isa::RegisterSelection base, int number);

%}

%option  noyywrap
%option yylineno

%option prefix="gpuasm_"

%%


[ \t\n]+ { } // ignore whitespace

"add" { return ADD; }
"mov" { return MOV; }
"swizzle" { return SWIZZLE; }
i[0-3] { gpuasm_lval.reg = calculateRegisterSelection(Isa::RegisterSelection::i0, atoi(&yytext[1])); return REG_INPUT; }
o[0-3] { gpuasm_lval.reg = calculateRegisterSelection(Isa::RegisterSelection::o0, atoi(&yytext[1])); return REG_OUTPUT; }
r[0-7] { gpuasm_lval.reg = calculateRegisterSelection(Isa::RegisterSelection::r0, atoi(&yytext[1])); return REG_GENERAL; }

[xyzw] {  return processVecComponent(yytext[0], gpuasm_lval);}
\.  { return DOT; }
"#input" { return HASH_INPUT; }
"#output" { return HASH_OUTPUT; }


. { return GPUASM_UNDEF; }


%%

static int processVecComponent(char c, GPUASM_STYPE &yyval) {
    switch(c) {
        case 'x': yyval.swizzleComponent = Isa::SwizzlePatternComponent::SwizzleX; return VEC_COMPONENT;
        case 'y': yyval.swizzleComponent = Isa::SwizzlePatternComponent::SwizzleY; return VEC_COMPONENT;
        case 'z': yyval.swizzleComponent = Isa::SwizzlePatternComponent::SwizzleZ; return VEC_COMPONENT;
        case 'w': yyval.swizzleComponent = Isa::SwizzlePatternComponent::SwizzleW; return VEC_COMPONENT;
        default: return GPUASM_error;
    }
}

static Isa::RegisterSelection calculateRegisterSelection(Isa::RegisterSelection base, int number) {
    int value = int(base) + number;
    return Isa::RegisterSelection(value);
}

void scannerSetParsedString(const char *str) {
    gpuasm__scan_string(str);
}

void scannerUnsetParsetString() {
    gpuasm__delete_buffer(YY_CURRENT_BUFFER);
}
