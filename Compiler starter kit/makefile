all: compiler.o parser.o error_handler.o lille_exception.o scanner.o id_table.o symbol.o token.o
	g++ -std=c++20 -o compiler compiler.o parser.o error_handler.o lille_exception.o scanner.o id_table.o symbol.o token.o
	echo Compilation complete.


compiler.o: parser.h scanner.h error_handler.h compiler.cpp
	g++ -std=c++20 -c compiler.cpp

parser.o: parser.h scanner.h token.h error_handler.h parser.cpp
	g++ -std=c++20 -c parser.cpp

error_handler.o: lille_exception.o token.o error_handler.h error_handler.cpp
	g++ -std=c++20 -c error_handler.cpp

id_table.o: id_table.h id_table.cpp
	g++ -std=c++20 -c id_table.cpp

lille_exception.o:	lille_exception.h lille_exception.cpp
	g++ -std=c++20 -c lille_exception.cpp

scanner.o: error_handler.o lille_exception.o token.o symbol.o id_table.o scanner.h scanner.cpp
	g++ -std=c++20 -c scanner.cpp

symbol.o: symbol.h symbol.cpp
	g++ -std=c++20 -c symbol.cpp

token.o: lille_exception.o symbol.o token.h token.cpp
	g++ -std=c++20 -c token.cpp

clean:
	rm -f *.o compiler
	echo Clean complete
