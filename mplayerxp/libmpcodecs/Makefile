include ../mp_config.mak

LIBNAME = libmpcodecs.a
SUBDIRS = libnuppelvideo liba52 libdca
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

CXXSRCS=dec_video.cpp vd.cpp ad.cpp dec_audio.cpp
CXXSRCS+=ad_a52.cpp ad_hwac3.cpp


CXXSRCS+=vd_lavc.cpp \
     vd_null.cpp \
     vd_nuv.cpp \
     vd_raw.cpp \
     vd_libmpeg2.cpp \
     vd_xvid.cpp \
     vd_mpegpes.cpp \
     vd_huffyuv.cpp \
     ad_dca.cpp \
     ad_faad.cpp \
     ad_lavc.cpp \
     ad_null.cpp \
     ad_mp3.cpp \
     ad_pcm.cpp \
     ad_dvdpcm.cpp \
     codecs_ld.cpp

ifeq ($(ENABLE_GPL_ONLY),no)
CXXSRCS+=vd_divx4.cpp \
     ad_real.cpp \
     vd_real.cpp \
     vd_xanim.cpp
endif
ifeq ($(HAVE_LIBVORBIS),yes)
CXXSRCS+=ad_vorbis.cpp
endif
ifeq ($(HAVE_LIBTHEORA),yes)
CXXSRCS+=vd_theora.cpp
endif
ifeq ($(HAVE_LIBDV),yes)
CXXSRCS+= ad_libdv.cpp vd_libdv.cpp
endif

ifeq ($(ENABLE_WIN32LOADER),yes)
CXXSRCS+=vd_dshow.cpp \
      vd_vfw.cpp \
      vd_dmo.cpp \
      vd_qtvideo.cpp \
      ad_dshow.cpp \
      ad_acm.cpp \
      ad_dmo.cpp \
      ad_qtaudio.cpp \
      ad_twin.cpp
endif

OBJS=$(SRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cpp=.o)

CFLAGS = $(OPTFLAGS) -I. -I.. -Wall
CXXFLAGS = $(OPTXXFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o

.PHONY: $(SUBDIRS)

all:	dep $(LIBNAME)
	
$(SUBDIRS):
	$(DO_ALL)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME): $(SUBDIRS) $(OBJS) $(CXXOBJS)
	$(AR) r $(LIBNAME) $(OBJS) $(CXXOBJS) \
	$(wildcard libnuppelvideo/*.o) \
	$(wildcard liba52/*.o) \
	$(wildcard libdca/*.o)
	

clean:
	$(DO_MAKE)
	rm -f *.o *.a *~

distclean:
	$(DO_MAKE)
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    .depend

.depend: $(SRCS) $(CXXSRCS)
	$(DO_MAKE)
	$(CXX) -MM $(CXXFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

