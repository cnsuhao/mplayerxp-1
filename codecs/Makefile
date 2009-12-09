# a BSD compatible 'install' program
INSTALL = install

include config.mak
BINDIR = $(LIBDIR)/$(PROGNAME)/codecs
SHLIBDIR = $(BINDIR)
# a BSD compatible 'install' program
INSTALL = install

DO_MAKE = @ for i in $(SUBDIRS); do SUBDIR=$@ && $(MAKE) -C $$i $@ || exit; done
LIB_NUPPELVIDEO=libnuppelvideo$(SLIBSUF)
SRC_NUPPELVIDEO=nuppelvideo.c RTjpegN.c minilzo.c
OBJ_NUPPELVIDEO=$(SRC_NUPPELVIDEO:.c=.o)
# just for: make dep
SRCS=$(SRC_NUPPELVIDEO)

CFLAGS  = $(MLIB_INC) $(OPTFLAGS)
ifneq ($(TARGET_ARCH_X86),yes)
CFLAGS += -fPIC
endif

.SUFFIXES: .c .o

all: $(SUBDIRS) $(LIB_NUPPELVIDEO)

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	export SUBDIR=ffmpeg liba52 libdca libmpeg2 mp3lib
	$(MAKE) -C $@

$(LIB_NUPPELVIDEO):	$(OBJ_NUPPELVIDEO)
	$(CC) --shared $(LDFLAGS) -o $@ $(OBJ_NUPPELVIDEO) -lc
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<


install:
	rm -f $(DESTDIR)$(BINDIR)/*$(SLIBSUF)
	$(DO_MAKE)
	$(INSTALL) -m 755 -s -p *$(DESTDIR)$(SLIBSUF) $(BINDIR)

clean:
	rm -f *.o *$(SLIBSUF)
	$(DO_MAKE)

distclean:
	$(DO_MAKE)
	rm -f config.h config.mak configure.log config.err
	rm -f *.o *$(SLIBSUF)

uninstall:
	$(DO_MAKE)
	rm -f $(DESTDIR)$(BINDIR)/*$(SLIBSUF)
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
