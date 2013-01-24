:- include('impldep.pro').

register(X)					--> [X], {number(X), between(0,31,X)}.
const(X)					--> [X], {number(X), between(-32768,32767,X)}.

sentence(S)					--> statement(S0), sentence_r(S0, S).
sentence_r(S, S)			--> [].
sentence_r(S0, seq(S0, S))	--> statement(S1), sentence_r(S1, S).

max(X)						--> const(X).
statement(instruction(X,Y))	--> max(X), [,], max(Y), ['\n'].