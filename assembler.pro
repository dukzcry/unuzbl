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
reg(X) -->
	nat(X), {register(X)}.
relative(Ptr) -->
	"(", reg(Ptr), ")".
whitespace -->
	" "; [].
comma -->
	whitespace, ",", whitespace.
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
statement(i(Op,0,0,D)) -->
	operator(Op), whitespace, label(D).
statement(i(Op,Dest,Src)) -->
	operator2(Op), whitespace, reg(Dest), comma, right(Src), "\n".
statement(i(Op,Rs,Rt,D)) -->
	operator3(Op), whitespace, reg(Rs), comma, reg(Rt), comma, label(D).
statement(X) -->
	label(X).
right(a(X)) -->
	"0", relative(X), !. % next
right(a(X,Off)) -->
	(nat(Off), {const(Off)}), relative(X).
right(d(X)) -->
	nat(X), {lim(X)}.
label(l(X)) -->
	":", nat(X), "\n".

operator(1) -->
	"ba".
operator2(2) -->
	"=".
operator3(3) -->
	"b=".

i(Opc,Rs,Y,_,L) :-
	functor(Y,d,1), arg(1,Y,Y1),
	L is storing immediate_word(Opc,Rs,0,Y1), !.
i(Opc,Rs,Y,_,L) :-
	functor(Y,a,2), arg(1,Y,Ra), arg(2,Y,D),
	L is storing immediate_word(Opc,Rs,Ra,D).
i(Opc,Rs,Rt,D,_,L) :-
	functor(D,l,1), arg(1,D,D1),
	recorded(D1,A,_),
	L is storing immediate_word(Opc,Rs,Rt,A).
l(X,PC,_) :-
	my_recordz(X,PC).

optimize(In,OutR) :-
	%reverse(In,InR),
	once(remove_dupes(In,OutR,0)).
	%reverse(Out,OutR)
remove_dupes([X|Xs0],[X|Xs],Line0) :-
	Line1 is Line0 + 1,
	(not(functor(X,l,1)) ; not(member(X,Xs0))),
	remove_dupes(Xs0,Xs,Line1).
remove_dupes([X|Xs0],Xs,Line0) :-
	Line1 is Line0 + 1,
	member(X,Xs0),
	erlang_writef('ignoring redefined ~w @ ~w~n',[X,Line0]),
	remove_dupes(Xs0,Xs,Line1).
remove_dupes([],[],_).
evaluate(X,Y) :-
	nonvar(X), evaluate(X,[],0,Y1),
	reverse(Y1,Y).
evaluate([X|Xs],LI,PC0,R) :-
	%writeln(PC0)
	call(X,PC0,W), (nonvar(W) ->
		LR = [W|LI], PC1 is PC0 + 1
			; !,
		LR = LI, PC1 is PC0),
	evaluate(Xs,LR,PC1,R).
evaluate([],L,_,L).
text(N) :-
	erlang_writef('~`0t~16r~8|~n',N).
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
	L = [Bs0,Bs1,Bs2,Bs3], F is storing flatten_diff(L).
	%writeln(F)
flatten_diff(S,F) :-
	nonvar(S),
	once(fd_binrec(S,F-[])).
fd_binrec([],X-X).
fd_binrec([X|Xs],Y-Z) :-
	fd_binrec(X,Y-T), fd_binrec(Xs,T-Z).
fd_binrec(X,[X|Z]-Z).
	

% rework: don't cut negative bit on truncate
binary_number(Bs0,N) :-
	nonvar(Bs0), reverse(Bs0,Bs),
	binary_number(Bs,0,0,N).
binary_number(N,Width,Bs0) :-
	nonvar(N), number(Width), (my_sign(N) =:= -1 ->
		Bit = 1, N1 is abs(N) - 1
			; !,
		Bit = 0, N1 = N),
	binary_number(Bs0,0,[],N1,Width,Bit).
binary_number([],_,N,N).
binary_number([B|Bs],I0,N0,N) :-
	between(0,1,B),
	% horner
	N1 is N0 + B * 2^I0, I1 is I0 + 1,
	binary_number(Bs,I1,N1,N).
binary_number(A,I,A,Q,Width,_) :- 
	Q < 1,
	% handling zero 
	I > 0,
	I >= Width, !. % loop
% above one was able to solve this task, but backtracing is too slow
binary_number(R,I0,L,N,Width,Bit) :-
	B is N mod 2,
	% inverted code
	B1 is abs(B - Bit),
	Q is N div 2, I1 is I0 + 1, 
	binary_number(R,I1,[B1|L],Q,Width,Bit).

%T is storing parse(In), To is storing optimize(T), Ws is storing evaluate(To),
%Out = "out.o", open(Out,write,Fd,[type(binary)]), set_output(Fd), dump(Ws,binary), told.
%Out = 'out.h', tell(Out), dump(Ws,text), told.
