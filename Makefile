OBJS = miv.o mivlayout.o mivselection.o

miv: $(OBJS)
	cc -o miv $(OBJS) `pkg-config --libs gtk+-3.0` -lm

%.o: %.c
	cc -Wall -c -o $*.o `pkg-config --cflags gtk+-3.0` $*.c
