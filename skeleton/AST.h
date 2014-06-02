// Written by Artem Falcon <lomka@gero.in>

#ifndef _ATREE_
#define _ATREE_

#include "lexer.h"
#include "shared.h"

class Operand: public Expression {
Value val;
public:
	Operand(Value v, int a): val(v), not_convert(0), is_f_name(0), is_arg(a) {}
	// Copy constructor
	Operand(const Operand &assigner, int a) {val = assigner.val;}
	Operand & operator = (const Operand &assigner) {
		// If assigner isn't current object itself,
		// i.e. their adresses doesn't match
		if(&assigner != this) val = assigner.val;
	}
	virtual Expression *clone() {return new Operand(*this);}
	virtual Value get_value() {return val;}
        virtual int is_what() {return 0;}

	int not_convert;
	int is_arg;
	int is_f_name;
};
class Binary: public Expression {
	int op;
	Expression *first, *sec;
public:
	Binary(Expression *f, Expression *s, int o): first(f), sec(s), op(o) {}
	Binary(const Binary &assigner) {
		first = assigner.first->clone();
		sec = assigner.sec->clone();
	}
	virtual ~Binary() {
		delete first; delete sec;
	}
	Binary & operator = (const Binary &assigner) {
		if(&assigner != this) {
			delete first; delete sec;
			first = assigner.first->clone();
			sec = assigner.sec->clone();
		}
	}
	virtual Expression *clone() {return new Binary(*this);}
	virtual Value get_value() {
		switch(op) {
		case clc_o::O_SUB:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) - *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) - *((float *) &sec->get_value());
		break;
                case clc_o::O_ADD:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) + *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) + *((float *) &sec->get_value());
			if(first->get_value().get_type() == 's' && sec->get_value().get_type() == 's')
				return first->get_value().get_str() + sec->get_value().get_str();
		break;
                case clc_o::O_MUL:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) * *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) * *((float *) &sec->get_value());
		break;
		case clc_o::O_DIV:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) / *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) / *((float *) &sec->get_value());
		case clc_o::O_MOD:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) % *((int *) &sec->get_value());
		break;
		case clc_o::O_SL: 
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) << *((int *) &sec->get_value());
		break;
		case clc_o::O_SR:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) >> *((int *) &sec->get_value());
		break;
		case clc_o::O_LO:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) < *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) < *((float *) &sec->get_value());
                        if(first->get_value().get_type() == 's' && sec->get_value().get_type() == 's')
                                return first->get_value().get_str() < sec->get_value().get_str();
		break;
		case clc_o::O_UP:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) > *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) > *((float *) &sec->get_value());
                        if(first->get_value().get_type() == 's' && sec->get_value().get_type() == 's')
                                return first->get_value().get_str() > sec->get_value().get_str();
		break;
		case clc_o::O_LE:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) <= *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) <= *((float *) &sec->get_value());
                        if(first->get_value().get_type() == 's' && sec->get_value().get_type() == 's')
                                return first->get_value().get_str() <= sec->get_value().get_str();
		break;
		case clc_o::O_BE:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) >= *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) >= *((float *) &sec->get_value());
                        if(first->get_value().get_type() == 's' && sec->get_value().get_type() == 's')
                                return first->get_value().get_str() >= sec->get_value().get_str();
		break;
		case clc_o::O_EE:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) == *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) == *((float *) &sec->get_value());
                        if(first->get_value().get_type() == 's' && sec->get_value().get_type() == 's')
                                return first->get_value().get_str() == sec->get_value().get_str();
		break;
		case clc_o::O_NE:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) != *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) != *((float *) &sec->get_value());
                        if(first->get_value().get_type() == 's' && sec->get_value().get_type() == 's')
                                return first->get_value().get_str() != sec->get_value().get_str();
		break;
		case clc_o::O_AND:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) & *((int *) &sec->get_value());
		break;
		case clc_o::O_XOR:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) ^ *((int *) &sec->get_value());
		break;
		case clc_o::O_OR:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) | *((int *) &sec->get_value());
		break;
		case clc_o::O_LAND:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) && *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) && *((float *) &sec->get_value());
		break;
		case clc_o::O_LOR:
                        if(first->get_value().get_type() == 'i' && sec->get_value().get_type() == 'i')
                                return *((int *) &first->get_value()) || *((int *) &sec->get_value());
                        if(first->get_value().get_type() == 'f' && sec->get_value().get_type() == 'f')
                                return *((float *) &first->get_value()) || *((float *) &sec->get_value());
		break;
		}
	}
	virtual int is_what() {return 'o';}
};
class Unary: public Expression {
	int op;
	Expression *contain;
public:
	Unary(Expression *c, int o): contain(c), op(o) {}
	Unary(const Unary &assigner) {
		contain = assigner.contain->clone();
	}
	virtual ~Unary() {delete contain;}
	Unary & operator = (const Unary &assigner) {
		if(&assigner != this) {
			delete contain;
			contain = assigner.contain->clone();
		}
	}
	virtual Expression *clone() {return new Unary(*this);}
	virtual Value get_value() {
		switch(op) {
		case clc_o::O_INC:
			if(contain->get_value().get_type() == 'i')
				return ++*((int *) &contain->get_value());
			if(contain->get_value().get_type() == 'f')
				return ++*((float *) &contain->get_value());
		break;
		case clc_o::O_DEC:
                        if(contain->get_value().get_type() == 'i')
                                return --*((int *) &contain->get_value());
                        if(contain->get_value().get_type() == 'f')
                                return --*((float *) &contain->get_value());
		break;
		case clc_o::O_NEG:
                        if(contain->get_value().get_type() == 'i')
                                return !*((int *) &contain->get_value());
                        if(contain->get_value().get_type() == 'f')
                                return !*((float *) &contain->get_value());
		break;	
		case clc_o::O_INV:
                        if(contain->get_value().get_type() == 'i')
                                return ~*((int *) &contain->get_value());
		break;
		case clc_o::O_MIN:
                        if(contain->get_value().get_type() == 'i')
                                return -*((int *) &contain->get_value());
                        if(contain->get_value().get_type() == 'f')
                                return -*((float *) &contain->get_value());
		break;
		}
	}
	virtual int is_what() {return 'o';}
};
class VFunc: public Expression {
        Expression *contain;
public:
        VFunc(Expression *c): contain(c) {}
        VFunc(const VFunc &assigner) {
                contain = assigner.contain->clone();
        }
        virtual ~VFunc() {delete contain;}
        VFunc & operator = (const VFunc &assigner) {
                if(&assigner != this) {
                        delete contain;
                        contain = assigner.contain->clone();
                }
        }
        virtual Expression *clone() {return new VFunc(*this);}
        virtual Value get_value() {
			if(contain->get_value().get_type() == 'i' ||
			contain->get_value().get_type() == 'f' ||
			contain->get_value().get_type() == 's')
				return contain->get_value();
        }
        virtual int is_what() {return 'v';}
};

class Cast: public Expression {
	std::string type;
	Expression *contain;
	std::string num2str(const float &n) 
	{std::ostringstream ss; ss << n; return ss.str();}
	float str2num(std::string s) 
	{std::istringstream ss(s); float v; ss >> v; return v;}
public:
	Cast(char *t, Expression *c): contain(c), type(t) {}
	Cast(const Cast &assigner) {
		contain = assigner.contain->clone();
	}
	virtual ~Cast() {delete contain; type="";}
	Cast & operator = (const Cast &assigner) {
		if(&assigner != this) {
			delete contain;
			contain = assigner.contain->clone();
		}
	}
	virtual Expression *clone() {return new Cast(*this);}
	virtual Value get_value() {
		if(type == "int" && contain->get_value().get_type() == 'i' ||
		type == "float" && contain->get_value().get_type() == 'f' ||
		type == "string" && contain->get_value().get_type() == 's'
		)
			return contain->get_value();
		if(type == "string" && contain->get_value().get_type() == 'i') {
			contain->not_convert = 1; std::string a;
			return a = num2str ( *( (int *) &contain->get_value()) );
		}
		if(type == "string" && contain->get_value().get_type() == 'f') {
			contain->not_convert = 1; std::string a;
			return a = num2str(*((float *) &contain->get_value()));
		} 
		if(type == "int" && contain->get_value().get_type() == 'f') {
			std::string a = num2str(*((float *) &contain->get_value()));
			std::string b=""; int i = 0; while(a.at(i) != '.') {b+=a[i];
			i++;}
			return (int) str2num(b);
		}
		if(type == "float" && contain->get_value().get_type() == 'i') {
			std::string a= num2str(*((int *) &contain->get_value())); 
			a.append(".0");
			return str2num(a);
		}
	}
	virtual int is_what() {return 'c';}
};
class TOp: public Expression {
	Expression *mexp, *fexp, *sexp;
public:
	TOp(Expression *m, Expression *f, Expression *s): mexp(m), fexp(f), sexp(s) {}
	TOp(const TOp &assigner) {
		mexp = assigner.mexp->clone();
		fexp = assigner.fexp->clone();
		sexp = assigner.sexp->clone();
	}
	//virtual ~TOp() {delete mexp;delete fexp;delete sexp;}
	TOp & operator = (const TOp &assigner) {
		if(&assigner != this) {
			delete mexp; delete fexp; delete sexp;
			mexp = assigner.mexp->clone();
			fexp = assigner.fexp->clone();
			sexp = assigner.sexp->clone();
		}
	}
	virtual Expression *clone() {return new TOp(*this);}
	virtual Value get_value() {
		if(mexp->get_value().get_type() == 'i' &&
			fexp->get_value().get_type() == 'i' &&
			sexp->get_value().get_type() == 'i') 
		{
			return ( *((int *) &mexp->get_value()) ) != 0
				?
			*((int *) &fexp->get_value()) :
			*((int *) &sexp->get_value());
		}
		if(mexp->get_value().get_type() == 'f' &&
			fexp->get_value().get_type() == 'f' &&
			sexp->get_value().get_type() == 'f')
		{
			return (*((float *) &mexp->get_value())) != 0
				?
			*((float *) &fexp->get_value()) :
			*((float *) &sexp->get_value());
		}
		if(mexp->get_value().get_type() == 's' &&
			fexp->get_value().get_type() == 's' &&
			sexp->get_value().get_type() == 's')
		{
			return mexp->get_value().get_str() != ""
				?
			fexp->get_value().get_str() : sexp->get_value().get_str();
		}
	}
	virtual int is_what() {return 't';}
};
class Asgn: public Expression {
	Expression *contain;
public:
	Asgn(Expression *c): contain(c) {}
	Asgn(const Asgn &assigner) {
		contain = assigner.contain->clone();
	}
	virtual ~Asgn() {delete contain;}
	Asgn & operator = (const Asgn &assigner) {
		if(&assigner != this) {
			delete contain;
			contain = assigner.contain->clone();
		}
	}
	virtual Expression *clone() {return new Asgn(*this);}
	virtual Value get_value() 
	{if(contain->get_value().get_type() == 'i' ||
         contain->get_value().get_type() == 'f' ||
         contain->get_value().get_type() == 's')
         return contain->get_value();}
	virtual int is_what() {return 'a';}
};
class Asgn2: public Expression {
	int op;
        Expression *contain;
public:
        Asgn2(Expression *c, int o): contain(c), op(o) {}
        Asgn2(const Asgn2 &assigner) {
                contain = assigner.contain->clone();
        }
        virtual ~Asgn2() {delete contain;}
        Asgn2 & operator = (const Asgn2 &assigner) {
                if(&assigner != this) {
                        delete contain;
                        contain = assigner.contain->clone();
                }
        }
        virtual Expression *clone() {return new Asgn2(*this);}
        virtual Value get_value() 
	{if(contain->get_value().get_type() == 'i' ||
         contain->get_value().get_type() == 'f' ||
         contain->get_value().get_type() == 's')
         return contain->get_value();
	}
        virtual int is_what() {return 'b';}
	virtual int res() {return op;}
};
class Func: public Expression {
        Expression *contain;
public:
        Func(Expression *c): contain(c) {}
        Func(const Func &assigner) {
                contain = assigner.contain->clone();
        }
        virtual ~Func() {delete contain;}
        Func & operator = (const Func &assigner) {
                if(&assigner != this) {
                        delete contain;
                        contain = assigner.contain->clone();
                }
        }
        virtual Expression *clone() {return new Func(*this);}
        virtual Value get_value() {return contain->get_value();}
        virtual int is_what() {return 'f';}
};
class Set: public Expression {
	Expression *contain;
public:
	Set(Expression *c): contain(c) {}
	Set(const Set &assigner) {
		contain = assigner.contain->clone();
	}
	virtual ~Set() {delete contain;}
	Set & operator = (const Set &assigner) {
		if(&assigner != this) {
			delete contain;
			contain = assigner.contain->clone();
		}
	}
	virtual Expression *clone() {return new Set(*this);}
	virtual Value get_value() {return contain->get_value();}
	virtual int is_what() {return 's';}
};

#endif /* ifndef */
