OBJS = miv.o mivlayout.o mivselection.o

miv: $(OBJS)
	cc -o miv $(OBJS) `pkg-config --libs gtk+-3.0` -lm

%.o: %.c
	cc -g -Wall -c -o $*.o `pkg-config --cflags gtk+-3.0` $*.c

clean:
	rm -f *.o
