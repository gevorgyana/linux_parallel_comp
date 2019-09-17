all:
	g++ -c -g main.cpp handlers.cpp functions.cpp
	g++ *.o -o app
