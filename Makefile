miv: miv.o
	cc -o miv miv.o `pkg-config --libs gtk+-3.0` -lm

miv.o: miv.c
	cc -c -o miv.o `pkg-config --cflags gtk+-3.0` miv.c
