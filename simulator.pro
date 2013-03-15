% keeping SWI-Prolog deps at min

:- include('shared.pro').
:- include('simulator.dep').

:- dynamic(unbytify_gen/4).

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
ram_load(Ram,A,M,N) :-
	%% octa-byte granularity
	ram_rule(M), align_rule(A,8),
	ram_sel(Ram,A,M,Bs), unbytify_gen(Bs,M,1,N), !. % next
ram_load(Ram,A,M,N) :-
	G is 8,
	%%
	ram_rule(M), once(align_bl(A,G,LB)), once(align_br(A,G,RB)),
	ram_sel(Ram,LB,G,HL), ram_sel(Ram,RB,G,LL),
	LS is A - LB, RS is RB - A,
	%% emulate HL << LS, LL >> RS
	LR =.. [r|HL], RR =.. [r|LL],
	ram_sel(LR,LS,RS,LBs), ram_sel(RR,0,LS,RBs),
	my_append(LBs,RBs,Bs),
	%%
	unbytify_gen(Bs,M,1,N).
ram_store(Ram,A,Bs,M,O) :-
	ram_rule(M),
	bytify_gen(Bs,M,R), ram_con(Ram,A,R,O).
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
unbytify_gen(Bs,N,S,O) :-
	T =.. [f|Bs], N1 is N div S,
	bagof(R,unbytify_elm(T,N1,S,R),O1),
	my_foldl(my_plus,O1,0,O),
	asserta((unbytify_gen(Bs,N,S,O) :- !)). % next
unbytify_elm(T,M,S,R) :-
	between(2,M,I), arg(I,T,V),
	R is V << (8 * S * (M - I)).
unbytify_elm(T,M,S,R) :-
	arg(1,T,V),
	R is ((V - ((V >> (8 * S - 1)) << (8 * S))) << (8 * S * (M - 1))).
align_bl(A,G,N) :-
	A1 is A - 1, X is A - G,
	between(X,A1,N), align_rule(N,G).
align_br(A,G,N) :-
	A1 is A + 1, X is A + G,
	between(A1,X,N), align_rule(N,G).

link(Cpu,O) :-	
	reg_sel(Cpu,pc,PC),	
	NI is PC + 1,	
	reg_con(Cpu,lr,NI,O).
