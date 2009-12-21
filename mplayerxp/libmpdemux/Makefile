
LIBNAME = libmpdemux.a

include ../mp_config.mak

DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

SRCS  = s_file.c s_ffmpeg.c
ifeq ($(HAVE_LIBCDIO_CDDA),yes)
SRCS += s_cdd.c cdda.c
ifeq ($(HAVE_STREAMING),yes)
SRCS += cddb.c
endif
endif
ifeq ($(USE_DVDNAV),yes)
SRCS += s_dvdnav.c
endif
ifeq ($(USE_DVDREAD),yes)
SRCS += s_dvdread.c
endif
SRCS  += s_tv.c  url.c
ifeq ($(USE_OSS_AUDIO),yes)
SRCS += s_oss.c
endif
ifeq ($(USE_LIBVCD),yes)
SRCS += s_vcdnav.c
endif
ifeq ($(HAVE_LIBCDIO),yes)
SRCS += s_vcd.c
endif
SRCS += cache2.c dvdauth.c stream.c tvi_dummy.c tvi_v4l.c
SRCS += tvi_bsdbt848.c frequencies.c mrl.c

SRCS += demuxer_r.c mp3_hdr.c video.c mpeg_hdr.c aviprint.c demux_asf.c
SRCS += demux_avi.c demux_mov.c parse_mp4.c demux_mpg.c demux_viv.c demuxer.c
SRCS += parse_es.c demux_fli.c demux_ra.c demux_real.c demux_y4m.c yuv4mpeg.c yuv4mpeg_ratio.c
SRCS += demux_nuv.c demux_film.c demux_roq.c demux_audio.c demux_demuxers.c
SRCS += mpdemux.c demux_ogg.c demux_bmp.c demux_rawaudio.c demux_aiff.c demux_vqf.c
SRCS += demux_pva.c demux_rawvideo.c demux_smjpeg.c demux_ts.c demux_ty.c demux_nsv.c
SRCS += demux_mkv.c demux_mpxp64.c demux_lavf.c sub_cc.c sub_ty.c

SRCS += muxer.c mux_mpxp64.c mux_lavf.c mux_raw.c
ifeq ($(HAVE_STREAMING),yes)
SUBDIRS = freesdp librtsp realrtsp
SRCS += asf_streaming.c asf_mmst_streaming.c cookies.c http.c network.c rtp.c pnm.c s_ftp.c s_udp.c s_rtsp.c s_network.c tcp.c udp.c
else
SUBDIRS =
endif

OBJS	= $(SRCS:.c=.o)
INCLUDE = -I../loader
CFLAGS  = $(OPTFLAGS) $(INCLUDE) -W -Wall

.SUFFIXES: .c .o

.PHONY: $(SUBDIRS)

all:	$(LIBNAME)

$(SUBDIRS):
	$(DO_ALL)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME): $(SUBDIRS) $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
ifeq ($(HAVE_STREAMING),yes)
	$(AR) r $(LIBNAME) \
	$(wildcard freesdp/*.o) \
	$(wildcard librtsp/*.o) \
	$(wildcard realrtsp/*.o)
endif

test:	$(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c ../mp_msg.c ../linux/shmem.c -o test -L. -lmpdemux -ldvdread -lz -lpthread

clean:
	$(DO_MAKE)
	rm -f *.o *.a *~

distclean:
	$(DO_MAKE)
	rm -f test Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(DO_MAKE)
	$(CC) -MM $(CFLAGS) test.c $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
