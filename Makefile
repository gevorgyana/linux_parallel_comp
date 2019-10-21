all:
	cc -g main.c handlers.c funcs.c  -I./include -L./lib -llab1 -o app

clean:
	rm *.o app
