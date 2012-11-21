include ../mp_config.mak

LIBNAME = libvo.a

CXXSRCS=video_out.cpp font_load.cpp img_format.cpp screenshot.cpp sub.cpp osd.cpp
SRCS=aspect.c vo_null.c
ifeq ($(HAVE_SDL),yes)
SRCS+=vo_sdl.c
endif
ifeq ($(HAVE_OPENGL),yes)
SRCS+=vo_opengl.c
endif
ifeq ($(HAVE_X11),yes)
SRCS+=vo_x11.c x11_common.c
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
CXXFLAGS  = $(OPTXXFLAGS) -I. -I.. -DMPG12PLAY -Wall -W

ifeq ($(CONFIG_VIDIX),yes)
CFLAGS += -DVIDIX_PATH='"$(LIBDIR)/vidix/"'
CXXFLAGS += -DVIDIX_PATH='"$(LIBDIR)/vidix/"'
endif

OBJS=$(SRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cpp=.o)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS) $(CXXOBJS)
	$(AR) r $(LIBNAME) $(OBJS) $(CXXOBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
