:- include('shared.pro').

reg_sel(Cpu,R,Val) :-
	register(R), 
	A is R + 1, arg(A,Cpu,Val), !.
reg_sel(T,F,V) :-
	find_fun(T,F,V,_).
reg_con(T,F,V,O) :-
	find_fun(T,F,_,N).
find_fun(Cpu,F,Val,N) :-
	T =.. [F,Val], arg(N,Cpu,T).
