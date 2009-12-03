# a BSD compatible 'install' program
INSTALL = install

include ../config.mak

LIBNAME = libmpeg2$(SLIBSUF)
BINDIR = $(LIBDIR)/$(PROGNAME)/codecs

SRCS	= alloc.c cpu_state.c decode.c header.c idct.c motion_comp.c slice.c
ifeq ($(TARGET_ARCH_X86),yes)
SRCS += idct_mmx.c motion_comp_mmx.c
endif
ifeq ($(TARGET_ARCH_X86_64),yes)
SRCS += idct_mmx.c motion_comp_mmx.c
endif
ifeq ($(TARGET_ARCH_ALPHA),yes)
SRCS += idct_alpha.c motion_comp_alpha.c
endif
ifeq ($(TARGET_MLIB),yes)
SRCS += idct_mlib.c motion_comp_mlib.c
endif
ifeq ($(TARGET_ALTIVEC),yes)
SRCS += idct_altivec.c motion_comp_altivec.c
endif

ifeq ($(TARGET_VIS),yes)
SRCS += motion_comp_vis.c
endif

OBJS	= $(SRCS:.c=.o)
INCLUDE = -I. -I../../mplayerxp/libvo -I.. $(EXTRA_INC) $(MLIB_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE) -DMPG12PLAY -Wall -W -Winline
ifneq ($(TARGET_ARCH_X86),yes)
CFLAGS += -fPIC
endif
ifeq ($(TARGET_ARCH_ALPHA),yes)
CFLAGS += -finline-limit=8000
OBJS += clamped_alpha.o
endif
.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(CC) --shared -o $@ $(OBJS) -lc

all:	$(LIBNAME)

install:
	$(INSTALL) -D -m 755 -p $(LIBNAME) $(DESTDIR)$(BINDIR)/$(LIBNAME)
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(LIBNAME)
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)
clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o $(LIBNAME) *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
