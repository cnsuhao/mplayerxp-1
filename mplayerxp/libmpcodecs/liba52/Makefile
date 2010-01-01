include ../../mp_config.mak

SRCS    = crc.c bit_allocate.c bitstream.c downmix.c imdct.c parse.c resample.c
OBJS	= $(SRCS:.c=.o)

CFLAGS  = $(MLIB_INC) $(OPTFLAGS) -W -Wall

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:	$(OBJS)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test *.o $(LIBNAME) *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
