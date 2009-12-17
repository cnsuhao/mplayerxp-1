# a BSD compatible 'install' program
INSTALL = install

include config.mak
BINDIR = $(LIBDIR)/$(PROGNAME)/codecs
SHLIBDIR = $(BINDIR)
# a BSD compatible 'install' program
INSTALL = install

DO_MAKE = @ for i in $(SUBDIRS); do SUBDIR=$@ && $(MAKE) -C $$i $@ || exit; done

CFLAGS  = $(MLIB_INC) $(OPTFLAGS)
ifneq ($(TARGET_ARCH_X86),yes)
CFLAGS += -fPIC
endif

.SUFFIXES: .c .o

all: $(SUBDIRS)

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	export SUBDIR=ffmpeg liba52 libdca libmpeg2 mp3lib
	$(MAKE) -C $@

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<


install:
	rm -f $(DESTDIR)$(BINDIR)/*$(SLIBSUF)
	$(DO_MAKE)

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
	$(DO_MAKE)

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
