:- include('shared.pro').

reg_sel(Cpu,R,Val) :-
	register(R), 
	A is R + 1, arg(A,Cpu,Val), !.
reg_sel(T,F,V) :-
	find_fun(T,F,V,_).
reg_con(T,F,V,O) :-
	find_fun(T,F,_,N), 
	T =.. L, L = [X|L1],
	% N is concretized :(
	change_val(L1,1,N,V, [],R),
	reverse(R,R1), O =.. [X|R1].
find_fun(Cpu,F,Val,N) :-
	T =.. [F,Val], arg(N,Cpu,T).
change_val([X|Xs],I,N,V,L,R) :-
	(I =\= N ->
		X1 = X
			; !,
		functor(X,F,_), X1 =.. [F,V]
	), I1 is I + 1,
	change_val(Xs,I1,N,V,[X1|L],R).
change_val([],_,_,_,L,L).