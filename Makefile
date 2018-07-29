OBJS = miv.o mivlayout.o mivselection.o items_creator.o

miv: $(OBJS)
	cc -o miv $(OBJS) `pkg-config --libs gtk+-3.0` -lm

%.o: %.c
	cc -g -Wall -c -o $*.o `pkg-config --cflags gtk+-3.0` $*.c

clean:
	rm -f *.o

dep:
	gccmakedep -f- -- `pkg-config --cflags gtk+-3.0` -- $(patsubst %.o,%.c,$(OBJS)) > .dep.new
	mv -f .dep.new .dep

-include .dep
