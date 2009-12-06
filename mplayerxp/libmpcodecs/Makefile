
include ../mp_config.mak

LIBNAME = libmpcodecs.a

SRCS=dec_video.c vd.c vd_null.c vd_ffmpeg.c vd_divx4.c vd_raw.c vd_nuv.c vd_libmpeg2.c vd_dshow.c vd_vfw.c vd_xvid.c vd_xanim.c vd_mpegpes.c vd_huffyuv.c vd_real.c vd_dmo.c vd_qtvideo.c vd_theora.c
SRCS+=ad.c dec_audio.c ad_null.c ad_ffmp3.c ad_mp3.c ad_a52.c ad_dca.c ad_hwac3.c ad_pcm.c ad_dvdpcm.c ad_dshow.c ad_acm.c ad_vorbis.c ad_real.c ad_dmo.c ad_qtaudio.c ad_faad.c ad_twin.c
SRCS+=codecs_ld.c

OBJS=$(SRCS:.c=.o)

CFLAGS = $(OPTFLAGS) -I. -I.. -I../libmpdemux -I../../codecs -I../../codecs/libavutil -I../loader -Wall 

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

