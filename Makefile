miv: miv.o mivlayout.o
	cc -o miv miv.o mivlayout.o `pkg-config --libs gtk+-3.0` -lm

%.o: %.c
	cc -c -o $*.o `pkg-config --cflags gtk+-3.0` $*.c
