include ../config.mak

LIBNAME = DMO_Filter$(SLIBSUF)
SLIBNAME = libDMO_Filter$(SLIBSUF)
BINDIR = $(LIBDIR)/$(PROGNAME)/wine

# DS_AudioDec.c 
SRCS	= DMO_AudioDecoder.c DMO_VideoDecoder.c  buffer.c  dmo.c  dmo_guids.c
OBJS = $(SRCS:.c=.o)

INCLUDE = -I. -I../dshow -I.. $(EXTRA_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE) -DNOAVIFILE_HEADERS

LD_LIBS = -lc
ifeq ($(TARGET_OS),WIN32)
LD=$(CC)
LD_LIBS += -lkernel32
LD_FLAGS=$(LDFLAGS) --dll --disable-runtime-pseudo-reloc --enable-auto-import --enable-auto-image-base --gc-sections --sort-common
else
LD_LIBS+=-L.. -lloader -L../dshow -lDS_Filter
LD_FLAGS=$(LDFLAGS) -Wl,-rpath=$(BINDIR)
endif


.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(SLIBNAME):	$(OBJS)
	$(CC) --shared $(LD_FLAGS) -o $@ $(OBJS) $(LD_LIBS)
all:	$(SLIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *$(SLIBSUF) *~ .depend test test.raw

install:
	install -D -m 755 -s -p $(SLIBNAME) $(DESTDIR)$(BINDIR)/$(SLIBNAME)
	ln -s -f $(BINDIR)/$(SLIBNAME) $(DESTDIR)$(BINDIR)/$(LIBNAME)
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(LIBNAME)
	rm -f $(DESTDIR)$(BINDIR)/$(SLIBNAME)
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

