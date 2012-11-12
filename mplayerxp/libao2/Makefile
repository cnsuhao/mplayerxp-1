
include ../mp_config.mak

LIBNAME = libao2.a

# TODO: moveout ao_sdl.c so it's only used when SDL is detected
SRCS= audio_out.c afmt.c ao_null.c ao_wav.c mixer.c
ifeq ($(HAVE_SDL),yes)
SRCS+=ao_sdl.c
endif
ifeq ($(USE_OSS_AUDIO),yes)
SRCS+=ao_oss.c
endif
ifeq ($(HAVE_ALSA),yes)
SRCS+=ao_alsa9.c
endif
ifeq ($(HAVE_ARTS),yes)
SRCS+=ao_arts.c
endif
ifeq ($(HAVE_ESD),yes)
SRCS+=ao_esd.c
endif
ifeq ($(HAVE_OPENAL),yes)
SRCS+=ao_openal.c
endif
ifeq ($(HAVE_NAS),yes)
SRCS+=ao_nas.c
endif
ifeq ($(HAVE_JACK),yes)
SRCS+=ao_jack.c
endif


OBJS=$(SRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall

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

