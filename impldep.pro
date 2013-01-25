% SWI Prolog

:- use_module(library(clpfd)).
run :-
	phrase_from_file(, 'in.asm', [buffer_size(16384)]).