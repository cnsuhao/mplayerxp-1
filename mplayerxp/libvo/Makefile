
include ../mp_config.mak

LIBNAME = libvo.a

SRCS=aspect.c aclib.c osd.c font_load.c video_out.c vo_null.c img_format.c sub.c screenshot.c
ifeq ($(HAVE_SDL),yes)
SRCS+=vo_sdl.c
endif
ifeq ($(HAVE_OPENGL),yes)
SRCS+=vo_opengl.c
endif
ifeq ($(HAVE_VDPAU),yes)
SRCS+=vo_vdpau.c
endif
ifeq ($(HAVE_X11),yes)
SRCS+=vo_x11.c x11_common.c
ifeq ($(CONFIG_VIDIX),yes)
SRCS+=vo_xvidix.c
endif
endif
ifeq ($(HAVE_XV),yes)
SRCS+=vo_xv.c
endif
ifeq ($(HAVE_DGA),yes)
SRCS+=vo_dga.c
endif
ifeq ($(HAVE_FBDEV),yes)
SRCS+=vo_fbdev.c
endif
ifeq ($(HAVE_VESA),yes)
SRCS += vo_vesa.c
endif
ifeq ($(CONFIG_VIDIX),yes)
SRCS += vosub_vidix.c
endif

CFLAGS  = $(OPTFLAGS) -I. -I.. -DMPG12PLAY -Wall -W
ifeq ($(CONFIG_VIDIX),yes)
CFLAGS += -DVIDIX_PATH='"$(LIBDIR)/vidix/"'
endif

OBJS=$(SRCS:.c=.o)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

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
