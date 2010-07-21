#ifndef _ACSHARED_
#define _ACSHARED_

class Value;
// User-defined.
class ExeObject {
public:
        virtual Value get_field(std::string) = 0;
        virtual void set_field(std::string, Value &) = 0;
        virtual Value do_call(std::string, std::vector <Value> &) = 0;
};
//---

class Value {
        union {
                int i_value;
                float f_value;
                ExeObject *o_value;
        };
        std::string s_value;
        std::set <Value> set_value;
        int type;
public:
        Value() {type = 'e';}
        Value(int i) {type = 'i'; i_value = i;}
        Value(float f) {type = 'f'; f_value = f;}
        Value(char *s) {type = 's'; s_value = s;}
        Value(ExeObject *o) {type = 'o'; o_value = o;}
        Value(std::string s) {type = 's'; s_value = s;}
        Value(std::set <Value> &st) {type = 'S'; set_value = st;}
        int get_type() {return type;}
        std::string get_str() {if(type == 's') return s_value; else return "";}
};

class Expression {
public:
        virtual ~Expression() {}
        // Clone objects of any type
        virtual Expression *clone() = 0;
        // Data for leaves, expression for nodes
        virtual Value get_value() = 0;
        // 0 - Operand (leave)                                
        // 'o' - Binary or Unary operation (node)
	// 'v' - Function without args
	// 'c' - Cast
	// 't' - ?: operator
	// 'a' - Asgn
	// 'b' - Asgn2
	// 'f' - Function
	// 's' - Set
	virtual int is_what() {return 1;}
	virtual int res() {} // Field instead produced an error
			     // while running :(
	int not_convert, is_arg, is_f_name;
};

#endif
