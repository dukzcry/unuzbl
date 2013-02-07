# Gmake Makefile

LDFLAGS=-lpthread
CXXFLAGS=-c -fpermissive
BFLAGS=-d -o
SRC=MemPool.cpp parser.cpp test.cpp
OBJ=$(SRC:.cpp=.o)
EXEC=test-app 

$(EXEC): bison all

default: bison
bison:
	bison parser.ypp $(BFLAGS) parser.tab.cpp
	sed -e "/int yyparse (void \*YYPARSE_PARAM);/d" -e "/int yyparse (void);/d" \
	-e "/int yyparse (void);/d" -e "/int yyparse ();/d" <parser.tab.cpp>parser.cpp

.cpp.o:
	$(CXX) $(CXXFLAGS) $(SRC)

all: $(OBJ)
	$(CXX) ${LDFLAGS} -o test-app $(OBJ)

clean:
	@echo 'Cleaning...'
	@rm parser*cpp parser.tab.hpp *.o $(EXEC) 2> /dev/null 
