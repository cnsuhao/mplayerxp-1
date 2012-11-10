# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

# a BSD compatible 'install' program
INSTALL = install

include mp_config.mak

PRG = $(PROGNAME)
PRG_FIBMAP = fibmap_mplayerxp
PRG_CFG = codec-cfg-xp
# these subdirectories required installation due binaries within them
SUBDIRS = libmpdemux libmpsub libplaytree libmpcodecs libmpconf libao2 osdep postproc input2 nls libvo xmpcore
ifeq ($(TARGET_ARCH_X86),yes)
SUBDIRS+=loader
endif
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_MAKE_ALL = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

MANDIR = ${prefix}/man
LDFLAGS += -Wl,-rpath,${CODECDIR}/codecs

SRCS_COMMON = mp_msg.c
SRCS_MPLAYER = mplayerxp.c fifo.c $(SRCS_COMMON) mixer.c mp-opt-reg.c dump.c

OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

FF_LIBS = ../ffmpeg/libavcodec/libavcodec$(FF_SUFFIX).a \
	  ../ffmpeg/libswscale/libswscale$(FF_SUFFIX).a \
	  ../ffmpeg/libswresample/libswresample$(FF_SUFFIX).a \
	  ../ffmpeg/libpostproc/libpostproc$(FF_SUFFIX).a \
	  ../ffmpeg/libavformat/libavformat$(FF_SUFFIX).a \
	  ../ffmpeg/libavutil/libavutil$(FF_SUFFIX).a

MP_LIBS = libmpdemux/libmpdemux.a \
	  libmpcodecs/libmpcodecs.a \
	  libmpsub/libmpsub.a \
	  libplaytree/libplaytree.a \
	  libao2/libao2.a \
	  postproc/libpostproc.a \
	  input2/libinput2.a \
	  libvo/libvo.a \
	  osdep/libosdep.a \
	  nls/libnls.a \
	  libmpconf/libmpconf.a \
	  xmpcore/libxmpcore.a
ifeq ($(TARGET_ARCH_X86),yes)
	  MP_LIBS += loader/libloader.a
endif

COMMON_LIBS = $(MP_LIBS) $(FF_LIBS) $(EXTRALIBS) -lm
CFLAGS = $(OPTFLAGS) $(EXTRA_INC)

ALL_PRG = $(PRG)
ifeq ($(HAVE_CSS),yes)
ALL_PRG += $(PRG_FIBMAP)
endif

.SUFFIXES: .c .o

# .PHONY: all clean

all:	$(ALL_PRG) $(SUBDIRS)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY: subdirs $(MP_LIBS)
subdirs: $(MP_LIBS)

$(PRG):	$(OBJS_MPLAYER) $(MP_LIBS)
	$(DO_MAKE_ALL)
	$(CC) -o $(PRG) $(OBJS_MPLAYER) $(COMMON_LIBS) $(LDFLAGS) $(LIBS)
#-Xlinker --export-dynamic -Xlinker --gc-sections -Xlinker --sort-common
$(SRCS_MPLAYER): version.h
#$(OBJS_MPLAYER): $(SRCS_MPLAYER)

$(PRG_FIBMAP): fibmap_mplayer.o
	$(CC) -o $(PRG_FIBMAP) fibmap_mplayer.o

$(PRG_CFG): version.h codec-cfg.c codec-cfg.h
	$(CC) $(CFLAGS) -g codec-cfg.c -o $(PRG_CFG) -DCODECS2HTML

install: $(ALL_PRG)
ifeq ($(INSTALL),)
	@echo "*** 'install' utility was not found and you can't run automatic"
	@echo "*** installation. Please download 'fileutils' from ftp://ftp.gnu.org and"
	@echo "*** install them to have possibility perform autiomatic installation"
	@echo "*** of this project" 
	@exit 1
endif
	$(INSTALL) -D -m 755 $(PRG) $(DESTDIR)$(BINDIR)/$(PRG)
	@if test ! -d $(DESTDIR)$(DATADIR) ; then mkdir -p $(DESTDIR)$(DATADIR) ; fi
	@if test ! -d $(DESTDIR)$(DATADIR)/font ; then mkdir -p $(DESTDIR)$(DATADIR)/font ; fi
	@if test ! -f $(DESTDIR)$(DATADIR)/font/font.desc ; then \
	echo "*** Download font at http://www.mplayerhq.hu/homepage/dload.html" ; \
	echo "*** for OSD/Subtitles support and extract to $(DATADIR)/font/" ; \
	fi

ifeq ($(HAVE_CSS),yes)
	@echo "Following task requires root privs. If it fails don't panic"
	@echo "however it means you can't use fibmap_mplayer."
	@echo "Without this (or without running mplayer as root) you won't be"
	@echo "able to play encrypted DVDs."
	-$(INSTALL) -D -o 0 -g 0 -m 4755 -s $(PRG_FIBMAP) $(DESTDIR)$(BINDIR)/$(PRG_FIBMAP)
endif

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PRG)
	rm -f $(DESTDIR)$(DATADIR)/font/*
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(DATADIR)/font
ifeq ($(HAVE_CSS),yes)
	rm -f $(DESTDIR)$(BINDIR)/$(PRG_FIBMAP)
endif
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)

clean:
	$(DO_MAKE)
	-rm -f *.o *~ $(OBJS)

distclean:
	$(DO_MAKE)
	-rm -f *~ $(PRG) $(PRG_FIBMAP) $(OBJS)
	-rm -f *.o *.a .depend configure.log
	-rm -f mp_config.h mp_config.mak version.h
	-rm -f cpuinfo help_mp.h

version.h:
	./version.sh `$(CC) --version`
	$(MAKE) dep

dep:
	$(CC) -MM $(CFLAGS) $(SRCS_MPLAYER) 1>.depend
	$(DO_MAKE)

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
