:- include('impldep.pro').

register(X) :-
	number(X), between(0,31,X).
% 16 bit consts only
const(X) :-
	number(X), between(-32768,32767,X).

call_semidet(Goal) :-
	call_nth(Goal,2) ->
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
negative(-1) -->
	"-".
negative(1) -->
	[].
nat(N) -->
	negative(D1), digit(D), {D1 =:= 1 ->
					D2 is D
						; !,
					D2 is -D}, 
	nat(D2,N).
nat(A,N) -->
	digit(D), {A1 is A * 10 + copysign(D,A)}, nat(A1,N).
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

% rework: don't cut negative bit on truncate
binary_common(Bs0,N,Width,Bit) :-
	reverse(Bs0,Bs),
	binary_number(Bs,0,0,N,Width,Bit).
binary_number(Bs0,N) :-
	nonvar(Bs0), atom_length(Bs0,Width),
	binary_common(Bs0,N,Width,0).
binary_number(Bs0,N,Width) :-
	( sign(N) =:= -1 ->
		Bit is 1, N1 is abs(N) - 1
			; !,
		Bit is 0, N1 is N ),
	binary_common(Bs0,N1,Width,Bit), !.
binary_number(_,I,N,N,Width,_) :-
	% handling zero
	I > 0,
	I >= Width.
binary_number([B|Bs],I0,N0,N,Width,Bit) :-
	between(0,1,B),
	% inverted code
	B1 is abs(B - Bit),
	% horner
	N1 #= N0 + B1 * 2^I0, I1 #= I0 + 1,
	binary_number(Bs,I1,N1,N,Width,Bit).