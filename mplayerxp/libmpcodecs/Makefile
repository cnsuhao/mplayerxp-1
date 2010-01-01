include ../mp_config.mak

LIBNAME = libmpcodecs.a
SUBDIRS = libnuppelvideo liba52 libdca
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

SRCS=dec_video.c \
     vd.c \
     vd_null.c \
     vd_ffmpeg.c \
     vd_divx4.c \
     vd_raw.c \
     vd_nuv.c \
     vd_libmpeg2.c \
     vd_xvid.c \
     vd_xanim.c \
     vd_mpegpes.c \
     vd_huffyuv.c \
     vd_real.c \
     vd_theora.c \
     ad.c \
     dec_audio.c \
     ad_null.c \
     ad_ffmp3.c \
     ad_mp3.c \
     ad_hwac3.c \
     ad_pcm.c \
     ad_dvdpcm.c \
     ad_real.c \
     ad_faad.c \
     ad_a52.c \
     ad_dca.c \
     codecs_ld.c
ifeq ($(HAVE_LIBVORBIS),yes)
SRCS+=ad_vorbis.c
endif
ifeq ($(HAVE_LIBTHEORA),yes)
SRCS+=vd_theora.c
endif

ifeq ($(HAVE_WIN32LOADER),yes)
SRCS+=vd_dshow.c \
      vd_vfw.c \
      vd_dmo.c \
      vd_qtvideo.c \
      ad_dshow.c \
      ad_acm.c \
      ad_dmo.c \
      ad_qtaudio.c \
      ad_twin.c
endif

OBJS=$(SRCS:.c=.o)

CFLAGS = $(OPTFLAGS) -I. -I.. -I../libmpdemux -I../loader -Wall

.SUFFIXES: .c .o

.PHONY: $(SUBDIRS)

all:	$(LIBNAME)
	
$(SUBDIRS):
	$(DO_ALL)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME): $(SUBDIRS) $(OBJS) 
	$(AR) r $(LIBNAME) $(OBJS) \
	$(wildcard libnuppelvideo/*.o) \
	$(wildcard liba52/*.o) \
	$(wildcard libdca/*.o)
	

clean:
	$(DO_MAKE)
	rm -f *.o *.a *~

distclean:
	$(DO_MAKE)
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(DO_MAKE)
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

