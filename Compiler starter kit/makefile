# makefile - builds the Lille compiler (scanner + parser + semantic analysis + symbol table)
# Student : Shlok Jain
# Course  : CSC-4510
#   make         build ./compiler
#   make test    rebuild and run the regression suite (tests/run_tests.sh)
#   make clean   remove object files and the executable

all: compiler.o parser.o error_handler.o lille_exception.o scanner.o id_table.o symbol.o token.o lille_type.o lille_kind.o
	g++ -std=c++20 -o compiler compiler.o parser.o error_handler.o lille_exception.o scanner.o id_table.o symbol.o token.o lille_type.o lille_kind.o
	echo Compilation complete.


compiler.o: parser.h scanner.h error_handler.h id_table.h compiler.cpp
	g++ -std=c++20 -c compiler.cpp

parser.o: parser.h scanner.h token.h error_handler.h id_table.h lille_type.h lille_kind.h parser.cpp
	g++ -std=c++20 -c parser.cpp

error_handler.o: lille_exception.o token.o error_handler.h error_handler.cpp
	g++ -std=c++20 -c error_handler.cpp

id_table.o: id_table.h id_table.cpp lille_type.h lille_kind.h token.h error_handler.h
	g++ -std=c++20 -c id_table.cpp

lille_type.o: lille_type.h lille_type.cpp
	g++ -std=c++20 -c lille_type.cpp

lille_kind.o: lille_kind.h lille_kind.cpp
	g++ -std=c++20 -c lille_kind.cpp

lille_exception.o:	lille_exception.h lille_exception.cpp
	g++ -std=c++20 -c lille_exception.cpp

scanner.o: error_handler.o lille_exception.o token.o symbol.o id_table.o scanner.h scanner.cpp
	g++ -std=c++20 -c scanner.cpp

symbol.o: symbol.h symbol.cpp
	g++ -std=c++20 -c symbol.cpp

token.o: lille_exception.o symbol.o token.h token.cpp
	g++ -std=c++20 -c token.cpp

test: all
	sh tests/run_tests.sh

clean:
	rm -f *.o compiler
	echo Clean complete
