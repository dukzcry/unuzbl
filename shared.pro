register(X) :-
        number(X), between(0,31,X).
bytify_word(Bs,Y1,Y2,Y3,Y4) :-
	X1 is Bs >> 16, Y1 is (X1 >> 8) 
	/\ 0xff, % kill sign
	Y2 is X1 /\ 0x00ff, X2 is Bs /\ 0x0000ffff, Y3 is X2 >> 8, Y4 is X2 /\ 0x00ff.
