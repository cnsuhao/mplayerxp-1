
include ../../config.mak

CFLAGS = -g -O2  -Wall -D__KERNEL__  -DMODULE -DSYNCFB_GENERIC_SUPPORT -DSYNCFB_MATROX_SUPPORT \
	 -include /usr/src/linux/include/linux/modversions.h -I/usr/src/linux/include

%CFLAGS = -g -O2  -Wall -D__KERNEL__ -D__SMP__ -DMODULE \
%	 -include /usr/src/linux/include/linux/modversions.h
OBJ = syncfb_base.o syncfb_matrox.o syncfb_generic.o
all: syncfb.o syncfb_test syncfb_off

syncfb.o : syncfb_matrox.o syncfb_generic.o syncfb_base.o
	$(LD) -r $^ -o $@

syncfb_test: syncfb_test.c
	$(CC) -g -O -Wall $(X11DIR) -lX11 $(INCLUDES) -o $@ $@.c

syncfbtv: syncfbtv.c
	$(CC) -g -O -Wall $(INCLUDES) -o $@ $@.c

syncfb_off: syncfb_off.c
	$(CC) -g -O -Wall $(INCLUDES) -o $@ $@.c

clean: 
	rm -f *.o
	rm -f syncfb_test syncfbtv syncfb_off

dep:

distclean: clean
	rm -f *~
	rm -f *.bak
	
