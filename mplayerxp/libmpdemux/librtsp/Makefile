include ../../mp_config.mak

SRCS = rtsp.c \
       rtsp_rtp.c  \
       rtsp_session.c
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I.. -I../.. $(EXTRA_INC) -Wall

.SUFFIXES: .c .o

all:    $(OBJS)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

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

