% SWI Prolog

run :- phrase_from_file(sentence(S), 'in.asm', [buffer_size(16384)]).