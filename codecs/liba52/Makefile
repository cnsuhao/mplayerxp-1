# a BSD compatible 'install' program
INSTALL = install

include ../config.mak

LIBNAME = liba52$(SLIBSUF)
BINDIR = $(LIBDIR)/$(PROGNAME)/codecs

SRCS    = crc.c bit_allocate.c bitstream.c downmix.c imdct.c parse.c resample.c
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

test:        $(LIBNAME) test.c
	$(CC) $(CFLAGS)  test.c ../../mplayerxp/mp_msg.c ../../mplayerxp/cpudetect.c -o test -L. -la52 -lm -Xlinker -rpath ./

all:	$(LIBNAME)

install:
	$(INSTALL) -D -m 755 -s -p $(DESTDIR)$(LIBNAME) $(BINDIR)/$(LIBNAME)
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
