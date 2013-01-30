:- include('impldep.pro').

register(X) :-
	number(X), between(0,31,X).
% 16 bit consts only
const(X) :-
	number(X), between(-32768,32767,X).

call_semidet(Goal) :-
	call_nth(Goal, 2) ->
		throw(error(mode_error(semidet,Goal),_))
			; !,
		once(Goal).
% determinism
/*my_phrase(NT) -->
	call(S0^S^call_semidet(phrase(NT,S0,S))).*/

% one look ahead
sentence(S) -->
	statement(S0), sentence_r(S0,S).
sentence_r(S0, sq(S0,S)) -->
	statement(S1), sentence_r(S1,S).
sentence_r(S, S) -->
	[].	

lim(X) :-
	const(X).
relative(Ptr) -->
	"(", (nat(Ptr), {register(Ptr)}), ")".
whitespace -->
	" "; [].
digit(0) --> "0". digit(1) --> "1". digit(2) --> "2".
digit(3) --> "3". digit(4) --> "4". digit(5) --> "5".
digit(6) --> "6". digit(7) --> "7". digit(8) --> "8".
digit(9) --> "9".
nat(N) -->
	digit(D), nat(D,N).
nat(A,N) -->
	digit(D), {A1 is A * 10 + D}, nat(A1,N).
nat(N,N) -->
	[].
% part_l =:= part_r
statement(i(Op,Dest,Src)) -->
	operator2(Op), whitespace, part(Dest), ",", whitespace, part(Src), "\n".
part(a(X)) -->
	"0", relative(X), !.
part(a(X,Off)) -->
	(nat(Off), {const(Off)}), relative(X).
part(d(X)) -->
	nat(X), {lim(X)}.

operator2(1) -->
	"=".

binary_common(Bs0,N,Upto) :- 
	reverse(Bs0,Bs),
	binary_number(Bs,0,0,N,Upto).
binary_number(Bs0,N) :-
	atom_length(Bs0,Upto),
	binary_common(Bs0,N,Upto).
binary_number(Bs0,N,Upto) :-
	binary_common(Bs0,N,Upto), !.
binary_number(_,I,N,N,Upto) :-
	I >= Upto.
binary_number([B|Bs],I0,N0,N,Upto) :-
	between(0,1,B),
	% horner
	N1 #= N0 + 2^I0 * B, I1 #= I0 + 1,
	binary_number(Bs,I1,N1,N,Upto).