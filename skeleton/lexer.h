#ifndef _LEXER_H_
#define _LEXER_H_

#include "headers.h"
#include "shared.h"

namespace clc_o {
enum Operations {
O_SUB,O_ADD,O_MUL,O_DIV,O_MOD,O_INC,O_DEC,
O_NEG,O_INV,O_MIN,O_SL,O_SR,O_LO,O_UP,O_LE,
O_BE,O_EE,O_NE,O_AND,O_XOR,O_OR,O_LAND,O_LOR,
O_SS,O_SV,O_SU,O_SD,O_SO,O_SAND,O_SXOR,O_SOR,
};
}
#define Letters (gram_letters = "\
-+*/%!~(),<>=&^|?:[]")

class ExeObject;
class ExeParser {
        std::string source_str, gram_letters;
	std::string tmp; // Buffer
	std::string::iterator ssci;
	int yyparse(), yylex(void *);
	void yyerror(const char *);
	char *set_func(char *);
	char *setr_func(char *, char *, char *);
public:
	ExeParser() {Letters;}
	void compile(std::string);
};
class ExeEval {
public:
	Value exec(ExeObject *, ExeObject *);
};

#define yyparse ExeParser::yyparse
#define yylex ExeParser::yylex
#define set_func ExeParser::set_func
#define setr_func ExeParser::setr_func

namespace clc_fvis {
char *pars_func(char *);
}

#endif
