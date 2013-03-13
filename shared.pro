register(X) :-
        number(X), between(0,31,X).
ram_rule(M) :-
	% 1,2,4,8 bytes
	between(1,8,M), E is 8 mod M, E =:= 0.
bytify_gen(Bs,N,O) :-
	ram_rule(N), M is N * 8,
	bytify_treerec(Bs,M,[],O).
bytify_treerec(N,M,Xs,R) :-
	M > 16,
	M1 is M div 2,
	NL is N >> M1, NR is N /\ (2 ^ M1 - 1), 
	bytify_treerec(NR,M1,Xs,RR), bytify_treerec(NL,M1,RR,R), !. % next
bytify_treerec(N,M,L,R) :-
	M =:= 16,
	Y1 is (N >> 8) 
	/\ 0xff, % kill sign
	Y2 is N /\ 0xff,
	R = [Y1,Y2|L], !. % next
bytify_treerec(N,_,L,R) :- NT is N /\ 0xff, R is [NT|L].
