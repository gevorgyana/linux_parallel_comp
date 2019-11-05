.PHONY: all clean run

all: ./app
./app:
	cc -g main.c handlers.c funcs.c  -I./include -L./lib/x86_64-linux -llab1 -o app
clean:
	rm *.o app

run: all
	LD_LIBRARY_PATH=lib/x86_64-linux/ ./app
