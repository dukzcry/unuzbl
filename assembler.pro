:- include('impldep.pro').

register(X) --> 
	[X], {number(X), between(0,31,X)}.
% 16 bit consts only
const(X) --> 
	[X], {number(X), between(-32768,32767,X)}.

% one look ahead
sentence(S) -->
	statement(S0), sentence_r(S0,S).
sentence_r(S, S) -->
	[].
sentence_r(S0, sq(S0,S)) -->
	statement(S1), sentence_r(S1,S).

lim(X) --> 
	const(X).
relative(Ptr) -->
	['('], register(Ptr), [')'].
whitespace -->
	[' ']; [].
% part_l =:= part_r
statement(i(Op,Dest,Src)) -->
	operator2(Op), whitespace, part(Dest), [,], whitespace, part(Src), ['\n'].
part(a(X)) -->
	[0], relative(X), !.
part(a(X,Off)) -->
	const(Off), relative(X).
part(d(X)) -->
	lim(X).

operator2(1) -->
	[=].

binary_number(Bs0,N,Upto) :-
	reverse(Bs0,Bs),
	binary_number(Bs,0,0,N,Upto), !.
binary_number(_,I,N,N,Upto) :-
	I = Upto.
binary_number([B|Bs],I0,N0,N,Upto) :-
	B in 0 .. 1,
	% horner
	N1 #= N0 + 2^I0 * B, I1 #= I0 + 1,
	binary_number(Bs,I1,N1,N,Upto).
