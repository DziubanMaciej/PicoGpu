%{
#include "inc.h"
#include <unistd.h>

static int processVecComponent(char c, GPUASM_STYPE &yyval);

%}

%option  noyywrap
%option yylineno

%option prefix="gpuasm_"

%%


[ \t\n]+ { } // ignore whitespace
"//".*     { } // ignore comments

"finit"   { return FINIT; }
"fadd"    { return FADD; }
"fsub"    { return FSUB; }
"fmul"    { return FMUL; }
"fdiv"    { return FDIV; }
"fneg"    { return FNEG; }
"fdot"    { return FDOT; }
"fcross"  { return FCROSS; }
"iinit"   { return IINIT; }
"iadd"    { return IADD; }
"isub"    { return ISUB; }
"imul"    { return IMUL; }
"idiv"    { return IDIV; }
"ineg"    { return INEG; }
"mov"     { return MOV; }
"swizzle" { return SWIZZLE; }

r[0-9]  { gpuasm_lval.reg = atoi(&yytext[1]); return REG; }
r1[0-5] { gpuasm_lval.reg = atoi(&yytext[1]); return REG; }

-?[0-9]+           { gpuasm_lval.i = atoi(yytext); return NUMBER_INT;   }
-?[0-9]+\.[0-9]*f? { gpuasm_lval.f = atof(yytext); return NUMBER_FLOAT; }

[xyzw] {  return processVecComponent(yytext[0], gpuasm_lval);}
\.  { return DOT; }
"#input" { return HASH_INPUT; }
"#output" { return HASH_OUTPUT; }
"#vertexShader" { return HASH_VS; }
"#fragmentShader" { return HASH_FS; }


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

void scannerSetParsedString(const char *str) {
    gpuasm__scan_string(str);
}

void scannerUnsetParsetString() {
    gpuasm__delete_buffer(YY_CURRENT_BUFFER);
}
