include ../../mp_config.mak

CXXSRCS=
SRCS = frequencies.c tvi_bsdbt848.c tvi_dummy.c tvi_v4l.c

OBJS=$(SRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cpp=.o)

CFLAGS  = $(OPTFLAGS) -I. -I../.. $(EXTRA_INC) -Wall
CXXFLAGS  = $(OPTXXFLAGS) -I. -I../.. $(EXTRA_INC) -Wall

.SUFFIXES: .c .o

all:    $(OBJS) $(CXXOBJS)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CC) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

