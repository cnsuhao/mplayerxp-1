
LIBNAME = libmpdemux.a

include ../mp_config.mak

SUBDIRS =
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

CXXSRCS=
SRCS=
ifeq ($(HAVE_LIBDV),yes)
SRCS += demux_dv.c
endif

SRCS += mp3_hdr.c video.c mpeg_hdr.c aviprint.c demux_asf.c
SRCS += demux_avi.c demux_mov.c parse_mp4.c demux_mpg.c demux_viv.c
SRCS += parse_es.c demux_fli.c demux_ra.c demux_real.c demux_y4m.c yuv4mpeg.c yuv4mpeg_ratio.c
SRCS += demux_nuv.c demux_film.c demux_roq.c demux_audio.c demux_demuxers.c
SRCS += demux_bmp.c demux_rawaudio.c demux_aiff.c demux_vqf.c
SRCS += demux_pva.c demux_rawvideo.c demux_smjpeg.c demux_ts.c demux_ty.c demux_nsv.c
SRCS += demux_mkv.c demux_mpxp64.c sub_cc.c sub_ty.c
SRCS += demux_lavf.c demux_null.c
ifeq ($(HAVE_LIBVORBIS),yes)
SRCS += demux_ogg.c
endif

SRCS += mux_mpxp64.c mux_raw.c
#SRCS+= mux_lavf.c

CXXSRCS+= demuxer.cpp demuxer_r.cpp mpdemux.cpp
CXXSRCS+= muxer.cpp

OBJS	= $(SRCS:.c=.o)
CXXOBJS	= $(CXXSRCS:.cpp=.o)
INCLUDE = -I../
CFLAGS  = $(OPTFLAGS) $(INCLUDE) -W -Wall
CXXFLAGS  = $(OPTXXFLAGS) $(INCLUDE) -W -Wall

.SUFFIXES: .c .o

.PHONY: $(SUBDIRS)

all:	$(LIBNAME)

$(SUBDIRS):
	$(DO_ALL)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME): $(SUBDIRS) $(OBJS) $(CXXOBJS)
	$(AR) r $(LIBNAME) $(OBJS) $(CXXOBJS)
ifeq ($(HAVE_STREAMING),yes)
	$(AR) r $(LIBNAME) \
	$(wildcard freesdp/*.o) \
	$(wildcard librtsp/*.o) \
	$(wildcard realrtsp/*.o)
endif

clean:
	$(DO_MAKE)
	rm -f *.o *.a *~

distclean:
	$(DO_MAKE)
	rm -f test Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(DO_MAKE)
	$(CC) -MM $(CFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
