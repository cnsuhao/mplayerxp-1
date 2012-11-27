include ../mp_config.mak

LIBNAME = libvo.a

CXXSRCS=video_out.cpp font_load.cpp img_format.cpp screenshot.cpp sub.cpp
CXXSRCS+=osd_render.cpp aspect.cpp vo_null.cpp
ifeq ($(HAVE_SDL),yes)
CXXSRCS+=vo_sdl.cpp
endif
ifeq ($(HAVE_OPENGL),yes)
CXXSRCS+=vo_opengl.cpp
endif
ifeq ($(HAVE_X11),yes)
CXXSRCS+=vo_x11.cpp x11_system.cpp
endif
ifeq ($(HAVE_XV),yes)
CXXSRCS+=vo_xv.cpp
endif
ifeq ($(HAVE_FBDEV),yes)
CXXSRCS+=vo_fbdev.cpp
endif
ifeq ($(HAVE_VESA),yes)
CXXSRCS += vo_vesa.cpp
endif
ifeq ($(CONFIG_VIDIX),yes)
CXXSRCS += vidix_system.cpp
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
	$(CXX) -MM $(CXXFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
