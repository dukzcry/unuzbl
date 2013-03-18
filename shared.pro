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
bytify_treerec(N,_,_,L,R) :- NT is N /\ 0xff, R = [NT|L].

% flatten_diff(+S,-F)
flatten_diff(S,F) :-
	once(fd_binrec(S,F-[])).
fd_binrec([],X-X).
fd_binrec([X|Xs],Y-Z) :-
	fd_binrec(X,Y-T), fd_binrec(Xs,T-Z).
fd_binrec(X,[X|Z]-Z).
my_append(A,B,L) :-
        %append_diff([A|W]-W,B-_,L1-[]), flatten_diff(L1,L).
        % DCG pokery
        expand_term((o --> A),X), arg(1,X,Y), arg(1,Y,Z), arg(2,Y,W),
        append_diff(Z-W,B-_,L-[]).
append_diff(A-B,B-C,A-C).
my_plus(X,Y,Z) :-
	Z is X + Y.
