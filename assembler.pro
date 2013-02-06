:- include('impldep.pro').

:- op(500,fx,storing).
storing(X,Y) :-
	Y = X.

register(X) :-
	number(X), between(0,31,X).
% 16 bit consts only
const(X) :-
	number(X), between(-32768,32767,X).
value_field(Bs,Val) :-
	binary_number(Val,16,Bs).
%
opcode(Bs,Opc) :-
	binary_number(Opc,6,Bs).
second_field(Bs,Val) :-
	binary_number(Val,5,Bs).

call_semidet(Goal) :-
	call_nth(Goal,2) ->
		throw(error(mode_error(semidet,Goal),_))
			; !,
		once(Goal).
% determinism
/*my_phrase(NT) -->
	call(S0^S^call_semidet(phrase(NT,S0,S))).*/

sentence(/*S*/[S0|S]) -->
	statement(S0), sentence(S).
	%sentence_r(S0,S)
sentence([]) -->
	[].
% one look ahead
/*sentence_r(S0,sq(S0,S)) -->
	statement(S1), sentence_r(S1,S).
sentence_r(S,S) -->
	[].*/

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
								D2 = D
									; !,
								D2 = -D}, 
	nat(D2,N).
nat(A,N) -->
	digit(D), {A1 is A * 10 + my_copysign(D,A)}, nat(A1,N).
nat(N,N) -->
	[].
statement(i(Op,Dest,Src)) -->
	operator2(Op), whitespace, left(Dest), ",", whitespace, right(Src), "\n".
left(d(X)) -->
	nat(X), {register(X)}.
right(a(X)) -->
	"0", relative(X), !.
right(a(X,Off)) -->
	(nat(Off), {const(Off)}), relative(X).
right(d(X)) -->
	nat(X), {lim(X)}.

operator2(1) -->
	"=".

i(Opc,X,Y,L) :-
	functor(X,d,1), functor(Y,d,1), arg(1,X,Rs), arg(1,Y,Y1),
	L is storing immediate_word(Opc,Rs,0,Y1), !.
i(Opc,X,Y,L) :-
	functor(X,d,1), functor(Y,a,2), arg(1,X,Rs), arg(1,Y,Ra), arg(2,Y,D),
	L is storing immediate_word(Opc,Rs,Ra,D).

evaluate(X,Y) :-
	nonvar(X), evaluate(X,[],Y1),
	reverse(Y1,Y).
evaluate([X|Xs],L,R) :-
	call(X,L1), evaluate(Xs,[L1|L],R).
evaluate([],L,L).
text(N) :-
	format('~`0t~16r~8|~n', N).
binary(N) :-
	write_word(N).
dump([Xs|Xss],Type) :-
	N is binary_number(Xs),
	T =.. [Type,N], call(T), dump(Xss,Type).
dump([],_).
write_word(Bs) :-
	X1 is Bs >> 16, Y1 is X1 >> 8, Y2 is X1 /\ 0x00ff,
	X2 is Bs /\ 0x0000ffff, Y3 is X2 >> 8, Y4 is X2 /\ 0x00ff,
	put_byte(Y1), put_byte(Y2),
	put_byte(Y3), put_byte(Y4).

immediate_word(Opc,Reg,Op,Val,F) :-
	opcode(Bs0,Opc), second_field(Bs1,Reg),
	Bs2 is storing binary_number(Op,5), value_field(Bs3,Val),
	L = [Bs0,Bs1,Bs2,Bs3], F is storing dflatten_rec(L).
	%writeln(F)
dflatten_rec(S,F) :-
  %nonvar(F)
  flatten_dl(S,F-[]), !.
flatten_dl([],X-X).
flatten_dl([X|Xs],Y-Z) :-
	flatten_dl(X,Y-T), flatten_dl(Xs,T-Z).
flatten_dl(X,[X|Z]-Z).

% rework: don't cut negative bit on truncate
binary_common(Bs0,N,Width,Bit) :-
	reverse(Bs0,Bs),
	binary_number(Bs,0,0,N,Width,Bit).
binary_number(Bs0,N) :-
	nonvar(Bs0), length(Bs0,Width),
	binary_common(Bs0,N,Width,0).
binary_number(N,Width,Bs0) :-
	nonvar(N), number(Width), (my_sign(N) =:= -1 ->
		Bit = 1, N1 is abs(N) - 1
			; !,
		Bit = 0, N1 = N),
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
	N1 is N0 + B1 * 2^I0, I1 is I0 + 1,
	binary_number(Bs,I1,N1,N,Width,Bit).

%T is storing parse(In), Ws is storing evaluate(T),
%Out = "out.o", open(Out,write,Fd,[type(binary)]), set_output(Fd), dump(Ws,binary), told.
%Out = 'out.h', tell(Out), dump(Ws,text), told.