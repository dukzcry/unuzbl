:- include('shared.pro').
:- include('impldep.pro').

:- op(500,fx,storing).
storing(X,Y) :-
	Y = X.
:- dynamic(binary_number/3).

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

reg(X) -->
	nat(X), {register(X)}.
val(X) -->
	nat(X), {const(X)}.
relative(Ptr) -->
	"(", reg(Ptr), ")".
whitespaces -->
	" ", whitespaces.
whitespaces -->
	[].
comma -->
	whitespaces, ",", whitespaces.
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
		D2 is -D}, 
	nat(D2,N).
nat(A,N) -->
	digit(D), {A1 is A * 10 + my_copysign(D,A)}, nat(A1,N).
nat(N,N) -->
	[].
statement(j(Op,D)) -->
	operator(Op), addr_common(D).
statement(i(Op,Dest,Src)) -->
	operator2(Op), whitespaces, reg(Dest), comma, right(Src), "\n".
statement(i(Op,Rs,Rt,D)) -->
	operator3(Op), whitespaces, reg(Rs), comma, reg(Rt), comma, addr_right(D), "\n".
statement(X) -->
	label(X), "\n".
right(a(X)) -->
	"0", relative(X), !. % next
right(a(X,Off)) -->
	val(Off), relative(X).
right(d(X)) -->
	val(X).
addr_common(X) -->
	whitespaces, addr_right(X), "\n".
addr_right(d(X)) -->
	val(X).
addr_right(X) -->
	label(X).
label(l(X)) -->
	":", nat(X).

operator(0) -->
	"ja".
operator(1) -->
	"j".
operator(2) -->
	"jl".
% rA = rB
operator2(3) -->
	"=".
% Ds(rB) = [rA], rB -= Ds
operator2(4) -->
	"sdu".
% rA = Ds(rB), rB += Ds
operator2(5) -->
	"ldu".
operator3(6) -->
	"j=".
operator3(7) -->
	"j!=".

% cuts for nexts
i(Opc,Rs,Y,_,L) :-
	functor(Y,d,1), arg(1,Y,Y1),
	L is storing immediate_word(Opc,Rs,0,Y1), !.
i(Opc,Rs,Y,_,L) :-
	functor(Y,a,2), arg(1,Y,Ra), arg(2,Y,D),
	L is storing immediate_word(Opc,Rs,Ra,D), !.
i(Opc,Rs,Y,_,L) :-
	/*functor(Y,a,1),*/ arg(1,Y,Ra),
	L is storing immediate_word(Opc,Rs,Ra,0).
i(Opc,Rs,Rt,D,PC,L) :-
	A is calc_absolute(D,PC),
	L is storing immediate_word(Opc,Rs,Rt,A), !.
i(Opc,Rs,Rt,D,PC,L) :-
	A is find_label(D,PC),
	L is storing immediate_word(Opc,Rs,Rt,A).
j(Opc,D,PC,L) :-
	between(1,2,Opc), A is calc_absolute(D,PC),
	L is storing jump_word(Opc,A), !.
j(0,D,_,L) :-
	functor(D,d,1), arg(1,D,A),
	L is storing jump_word(0,A), !.
j(Opc,D,PC,L) :-
	A is find_label(D,PC),
	L is storing jump_word(Opc,A).
find_label(D,PC,A) :-
	/*functor(D,l,1),*/ arg(1,D,D1),
	(A is my_recorded(D1,_);
	throw(error(mode_error('undefined reference to',D1,'PC=',PC),_))), !. % once
calc_absolute(D,PC,A) :-
	functor(D,d,1), arg(1,D,D1), A is PC + D1.

preevaluate(In,OutR) :-
	%reverse(In,InR),
	once(remove_dupes(In,OutR,0)),
	%reverse(Out,OutR)
	assert(l(_,_,_)).
remove_dupes([X|Xs0],[X|Xs],PC0) :-
	(\+(functor(X,l,1)) ; \+(member(X,Xs0))),
	(functor(X,l,1) ->
		arg(1,X,X1), my_recordz(X1,PC0), PC1 is PC0
			; !,
		PC1 is PC0 + 1),
	remove_dupes(Xs0,Xs,PC1).
remove_dupes([X|Xs0],Xs,PC) :-
	erlang_writef('ignoring redefined ~w, PC=~w~n',[X,PC]),
	remove_dupes(Xs0,Xs,PC).
remove_dupes([],[],_).
% evaluate(+X,-Y)
evaluate(X,Y) :-
	evaluate(X,[],0,Y1), reverse(Y1,Y).
% custom maplist
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
	bytify_word(Bs,Y1,Y2,Y3,Y4),
	put_byte(Y1), put_byte(Y2), put_byte(Y3), put_byte(Y4).

immediate_word(Opc,Reg,Op,Val,F) :-
	opcode(Bs0,Opc), second_field(Bs1,Reg),
	Bs2 is storing binary_number(Op,5), value_field(Bs3,Val),
	L = [Bs0,Bs1,Bs2,Bs3], F is storing flatten_diff(L).
jump_word(Opc,A,F) :-
	opcode(Bs0,Opc), Bs1 is storing binary_number(A,26),
	L = [Bs0,Bs1], F is storing flatten_diff(L).
% flatten_diff(+S,-F)
flatten_diff(S,F) :-
	once(fd_binrec(S,F-[])).
fd_binrec([],X-X).
fd_binrec([X|Xs],Y-Z) :-
	fd_binrec(X,Y-T), fd_binrec(Xs,T-Z).
fd_binrec(X,[X|Z]-Z).
	
% rework: don't cut negative bit on truncate
% binary_number(+Bs0,-N)
binary_number(Bs0,N) :-
	reverse(Bs0,Bs), binary_number(Bs,0,0,N).
% binary_number(+N,+Width,-Bs0)
binary_number(N,Width,Bs0) :-
	(my_sign(N) =:= -1 ->
		Bit = 1, N1 is abs(N) - 1
			; !,
		Bit = 0, N1 = N),
	binary_number(Bs0,0,[],N1,Width,Bit),
	asserta(binary_number(N,Width,Bs0) :- !). % next
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

%T is storing parse(In), Tp is storing preevaluate(T), Ws is storing evaluate(Tp),
%Out = "out.o", open(Out,write,Fd,[type(binary)]), set_output(Fd), dump(Ws,binary), told.
%Out = 'out.h', tell(Out), dump(Ws,text), told.
