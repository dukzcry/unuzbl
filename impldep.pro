% SWI Prolog

:- use_module(library(clpfd)).
% http://www.complang.tuwien.ac.at/ulrich/Prolog-inedit/lambda.pl
%:- use_module(lambda).

run :-
	phrase_from_file(sentence(AST), 'in.asm', [buffer_size(16384)]).
call_nth(Goal, C) :-
	State = count(0,_), Goal, arg(1, State, C1), 
	C2 is C1 + 1, nb_setarg(1, State, C2), C = C2.