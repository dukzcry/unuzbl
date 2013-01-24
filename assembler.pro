:- include('impldep.pro').

% 32 GP registers
register(X)
	--> [X], {number(X), between(0,31,X)}.
% 16 bit consts only
const(X)
	--> [X], {number(X), between(-32768,32767,X)}.

% one look ahead
sentence(S)
	--> statement(S0), sentence_r(S0, S).
sentence_r(S, S)
	--> [].
sentence_r(S0, sq(S0, S))
	--> statement(S1), sentence_r(S1, S).

% part_l =:= part_r
statement(i(Op,Dest,Src))
	--> operator2(Op), part(Dest), [,], part(Src), ['\n'].
lim(X)
	--> const(X).
relative(Ptr)
	--> ['('], register(Ptr), [')'].
part(a(X))
	--> [0], relative(X), !.
part(a(X,Off))
	--> const(Off), relative(X).
part(d(X))
	--> lim(X).

operator2(1)
	--> [li].