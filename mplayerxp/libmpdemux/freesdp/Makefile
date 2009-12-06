include ../../mp_config.mak

LIBNAME = libfreesdp.a

SRCS = common.c \
       errorlist.c  \
       parser.c
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I../.. $(EXTRA_INC) -Wall
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

all:    $(LIBNAME)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)


clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

