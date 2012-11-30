LIBNAME = libmpdemux.a

include ../mp_config.mak

SUBDIRS =
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

CXXSRCS=
SRCS=
ifeq ($(HAVE_LIBDV),yes)
CXXSRCS += demux_dv.cpp
endif

CXXSRCS += mp3_hdr.cpp video.cpp mpeg_hdr.cpp aviprint.cpp parse_mp4.cpp parse_es.cpp
CXXSRCS += yuv4mpeg.cpp yuv4mpeg_ratio.cpp sub_cc.cpp sub_ty.cpp
CXXSRCS += demux_aiff.cpp \
           demux_asf.cpp \
           demux_audio.cpp \
           demux_avi.cpp \
           demux_bmp.cpp \
           demux_demuxers.cpp \
           demux_film.cpp \
           demux_fli.cpp \
           demux_lavf.cpp \
           demux_mkv.cpp \
           demux_mov.cpp \
           demux_mpg.cpp \
           demux_mpxp64.cpp \
           demux_nsv.cpp \
           demux_null.cpp \
           demux_nuv.cpp \
           demux_pva.cpp \
           demux_rawaudio.cpp \
           demux_rawvideo.cpp \
           demux_real.cpp \
           demux_realaud.cpp \
           demux_roq.cpp \
           demux_smjpeg.cpp \
           demux_ts.cpp \
           demux_ty.cpp \
           demux_viv.cpp \
           demux_vqf.cpp \
           demux_y4m.cpp
ifeq ($(HAVE_LIBVORBIS),yes)
CXXSRCS += demux_ogg.cpp
endif

CXXSRCS += mux_mpxp64.cpp mux_raw.cpp
#SRCS+= mux_lavf.c

CXXSRCS+= demuxer.cpp demuxer_packet.cpp demuxer_stream.cpp demuxer_r.cpp
CXXSRCS+= muxer.cpp mpdemux.cpp

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
	$(CXX) -MM $(CXXFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
