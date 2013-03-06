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


jl(Cpu,A,O) :-
	reg_sel(Cpu,pc,PC),
	NI is PC + 1,
	reg_con(Cpu,[lr,pc],[NI,A],O).
