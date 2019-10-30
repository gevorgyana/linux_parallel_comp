all:
	cc -g main.c handlers.c funcs.c  -I./include -L./lib/x86_64-linux -llab1 -o app
clean:
	rm *.o app
