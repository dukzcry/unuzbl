reg_sel(C,PC,O) :-
        arg(_,C,pc(O)), !.
reg_sel(C,R,O) :-
        A is R + 1, arg(A,C,O).
