include ../../mp_config.mak

SRCS = menu.c \
       menu_cmdlist.c  \
       menu_pt.c \
       menu_list.c  \
       menu_filesel.c \
       menu_txt.c \
       menu_console.c \
       menu_param.c 
OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I../.. $(EXTRA_INC) -Wall

.SUFFIXES: .c .o

all:  $(OBJS)

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

