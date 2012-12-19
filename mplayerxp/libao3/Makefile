
include ../mpxp_config.mak

LIBNAME = libao3.a

# TODO: moveout ao_sdl.c so it's only used when SDL is detected
CXXSRCS=audio_out.cpp afmt.cpp
CXXSRCS+=ao_null.cpp ao_wav.cpp
ifeq ($(HAVE_SDL),yes)
CXXSRCS+=ao_sdl.cpp
endif
ifeq ($(USE_OSS_AUDIO),yes)
CXXSRCS+=ao_oss.cpp
endif
ifeq ($(HAVE_ALSA),yes)
CXXSRCS+=ao_alsa.cpp
endif
ifeq ($(HAVE_ARTS),yes)
CXXSRCS+=ao_arts.cpp
endif
ifeq ($(HAVE_ESD),yes)
CXXSRCS+=ao_esd.cpp
endif
ifeq ($(HAVE_OPENAL),yes)
CXXSRCS+=ao_openal.cpp
endif
ifeq ($(HAVE_NAS),yes)
CXXSRCS+=ao_nas.cpp
endif
ifeq ($(HAVE_JACK),yes)
CXXSRCS+=ao_jack.cpp
endif

OBJS=$(SRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cpp=.o)

CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall
CXXFLAGS  = $(OPTXXFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS) $(CXXOBJS)
	$(AR) r $(LIBNAME) $(OBJS) $(CXXOBJS)

all:    dep $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    .depend

.depend: $(SRCS) $(CXXSRCS)
	$(CXX) -MM $(CXXFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

