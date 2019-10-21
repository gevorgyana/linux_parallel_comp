all:
	cc -c -g main.c handlers.c
	gcc *.o -o app

clean:
	rm *.o app
