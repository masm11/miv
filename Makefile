OBJS = miv.o mivlayout.o mivselection.o items_creator.o movie.o

miv: $(OBJS)
	cc -o miv $(OBJS) `pkg-config --libs gstreamer-1.0 gtk+-3.0` -lm

%.o: %.c
	cc -g -Wall -c -o $*.o `pkg-config --cflags gstreamer-1.0 gtk+-3.0` $*.c

clean:
	rm -f *.o

dep:
	gccmakedep -f- -- `pkg-config --cflags gstreamer-1.0 gtk+-3.0` -- $(patsubst %.o,%.c,$(OBJS)) > .dep.new
	mv -f .dep.new .dep

-include .dep

test: test.c
	cc -o $@ `pkg-config --cflags gstreamer-1.0 gtk+-3.0` test.c `pkg-config --libs gstreamer-1.0 gtk+-3.0`
