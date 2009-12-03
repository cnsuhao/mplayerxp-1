include ../config.mak

LIBNAME = DS_Filter$(SLIBSUF)
SLIBNAME = libDS_Filter$(SLIBSUF)
BINDIR = $(LIBDIR)/$(PROGNAME)/wine

# DS_AudioDec.c 
SRCS	= DS_AudioDecoder.c DS_Filter.c DS_VideoDecoder.c allocator.c cmediasample.c guids.c inputpin.c mediatype.c outputpin.c aviprint.c
OBJS = $(SRCS:.c=.o)

# OBJS	= DS_AudioDec.o DS_VideoDec.o DS_Filter.o allocator.o cmediasample.o guids.o inputpin.o outputpin.o

INCLUDE = -I. -I.. $(EXTRA_INC) -DNOAVIFILE_HEADERS
CFLAGS  = $(OPTFLAGS) $(INCLUDE)

LD_LIBS = -lc
ifeq ($(TARGET_OS),WIN32)
LD=$(CC)
LD_LIBS+=-lkernel32 -ladvapi32 -lole32
LD_FLAGS=$(LDFLAGS)
# -Wl,--output-def,$(@:.dll=.def),--out-implib,lib$(DS_Filter:$(SLIBSUF)=.dll.a)
LD_FLAGS+=--dll --disable-runtime-pseudo-reloc --enable-auto-import --enable-auto-image-base --export-all-symbols --gc-sections --sort-common
else
LD_LIBS+=-L.. -lloader
LD_FLAGS=$(LDFLAGS) -Wl,-rpath=$(BINDIR)
endif

.SUFFIXES: .c .o

# .PHONY: all clean

all:	$(SLIBNAME)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(SLIBNAME):	$(OBJS)
	$(CC) --shared $(LD_FLAGS) -o $(SLIBNAME) $(OBJS) $(LD_LIBS)

test:   test.c $(LIBNAME)
	$(CC) test.c -Wall $(CFLAGS) -o test -L. -l$(SLIBNAME) -L.. -lloader $(ARCH_LIBS) -lstdc++

clean:
	rm -f *.o $(LIBNAME) *~

distclean:
	rm -f Makefile.bak *.o $(LIBNAME) *~ .depend test test.raw

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

install:
	install -D -m 755 -s -p $(SLIBNAME) $(DESTDIR)$(BINDIR)/$(SLIBNAME)
	ln -s -f $(BINDIR)/$(SLIBNAME) $(DESTDIR)$(BINDIR)/$(LIBNAME)
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(LIBNAME)
	rm -f $(DESTDIR)$(BINDIR)/$(SLIBNAME)
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)
#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

