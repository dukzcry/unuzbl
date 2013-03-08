% minimum SWI-Prolog deps, we may wish to compile this code with GNU Prolog for speedup in future

:- include('shared.pro').

reg_sel(Cpu,R,V) :-
	reg_arg(R,A), arg(A,Cpu,V), !. % next
reg_sel(T,F,V) :-
	once(find_fun(T,F,V,_)).
% wrapper
reg_con(T,[F|Fs],[V|Vs],O) :-
	reg_con(T,F,V,O1), reg_con(O1,Fs,Vs,O), !. % next
reg_con(T,F,V,O) :-
	(reg_arg(F,N) ; find_fun(T,F,_,N)), 
	T =.. L, L = [X|L1],
	% avoid using setarg/3
	with_val(L1,1,N,V, [],R),
	reverse(R,R1), O =.. [X|R1], !. % once
% wrapper
reg_con(O,[],[],O).
ram_sel(Ram,A,N,O) :-
	A1 is A + N - 1,
	ram_sel(Ram,A1,N,0,[],O).
ram_sel(R,A,N,I,V,O) :-
	ram_sel(R,A,V1),
	A1 is A - 1, I1 is I + 1,
	ram_sel(R,A1,N,I1,[V1|V],O), !. % next
ram_sel(_,_,N,N,O,O).
ram_sel(Ram,A,V) :-
	N is A + 1, arg(N,Ram,V).
% wrapper
ram_con(R,A,N,O) :-
	number(N),
	ram_con(R,A,[N],O), !. % next
ram_con(Ram,A,[V|Vs],O) :-
	N is A + 1,
	% mutation cause ram is too big
	setarg(N,Ram,V),
	ram_con(Ram,N,Vs,O), !. % once
ram_con(O,_,[],O).
ram_load(Ram,A,N) :-
	ram_sel(Ram,A,8,Bs),
	Bs = [Y1,Y2,Y3,Y4,Y5,Y6,Y7,Y8],
	% replace this crap with generator
	N is (((Y1 - (Y1 >> 7) << 8) << 56) \/ (Y2 << 48) \/ (Y3 << 40) \/ (Y4 << 32) \/ (Y5 << 24) \/ (Y6 << 16)) \/ (Y7 << 8) \/ Y8.
ram_store(Ram,A,Bs,O) :-
	Bs1 is Bs >> 32, bytify_word(Bs1,Y1,Y2,Y3,Y4),
	Bs2 is Bs /\ 0x00000000ffffffff, bytify_word(Bs2,Y5,Y6,Y7,Y8),
	ram_con(Ram,A,[Y1,Y2,Y3,Y4,Y5,Y6,Y7,Y8],O).
reg_arg(R,A) :-
	register(R), 
	A is R + 1.
find_fun(Cpu,F,Val,N) :-
	T =.. [F,Val], arg(N,Cpu,T).
with_val([X|Xs],I,N,V,L,R) :-
	(I =\= N ->
		X1 = X
			; !,
		(register(X), X1 = V
			;
		functor(X,F,_), X1 =.. [F,V])
	), I1 is I + 1,
	with_val(Xs,I1,N,V,[X1|L],R).
with_val([],_,_,_,L,L).

link(Cpu,O) :-	
	reg_sel(Cpu,pc,PC),	
	NI is PC + 1,	
	reg_con(Cpu,lr,NI,O).
