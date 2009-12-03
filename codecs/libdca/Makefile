# a BSD compatible 'install' program
INSTALL = install

include ../config.mak

LIBNAME = libdca$(SLIBSUF)
BINDIR = $(LIBDIR)/$(PROGNAME)/codecs

SRCS    = bitstream.c downmix.c parse.c resample.c
OBJS	= $(SRCS:.c=.o)

CFLAGS  = $(MLIB_INC) $(OPTFLAGS) -W -Wall
ifneq ($(TARGET_ARCH_X86),yes)
CFLAGS += -fPIC
endif

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(CC) --shared -o $@ $(OBJS) -lm -lc

all:	$(LIBNAME)

install:
	$(INSTALL) -D -m 755 -s -p $(LIBNAME) $(DESTDIR)$(BINDIR)/$(LIBNAME)
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(LIBNAME)
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)
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
