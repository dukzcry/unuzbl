register(X) :-
        number(X), between(0,31,X).
ram_rule(M) :-
	% 1,2,4,8 bytes
	between(1,8,M), E is 8 mod M, E =:= 0.
align_rule(A,G) :-
	A mod G =:= 0.
bytify_gen(Bs,N,O) :-
	M is N * 8, bytify_treerec(Bs,M,1,[],O).
%% algorithmic, don't swap
/*bytify_treerec(N,M,G,Xs,R) :-
	N1 is N /\ (2 ^ M - 1), % kill sign
	M div 8 =:= G, R = [N1|Xs], !. % next
*/
bytify_treerec(N,M,S,Xs,R) :-
	M > 16,
	M1 is M div 2,
	NL is N >> M1, NR is N /\ (2 ^ M1 - 1),
	bytify_treerec(NR,M1,S,Xs,RR), bytify_treerec(NL,M1,S,RR,R), !. % next
%%
bytify_treerec(N,M,_,L,R) :-
	M =:= 16,
	Y1 is (N >> 8) 
	/\ 0xff, % kill sign
	Y2 is N /\ 0xff,
	R = [Y1,Y2|L], !. % next
bytify_treerec(N,_,_,L,R) :- NT is N /\ 0xff, R is [NT|L].
