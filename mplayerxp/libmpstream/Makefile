
LIBNAME = libmpstream.a

include ../mp_config.mak

DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

CXXSRCS=s_tv.cpp
CXXSRCS+= s_file.cpp s_ffmpeg.cpp s_null.cpp
ifeq ($(HAVE_LIBCDIO_CDDA),yes)
CXXSRCS += s_cdd.cpp
CXXSRCS += cdda.cpp
ifeq ($(HAVE_STREAMING),yes)
CXXSRCS += cddb.cpp
endif
endif
ifeq ($(USE_DVDNAV),yes)
CXXSRCS += s_dvdnav.cpp
endif
ifeq ($(USE_DVDREAD),yes)
SRCS += s_dvdread.c
endif
SRCS  += url.c
ifeq ($(USE_OSS_AUDIO),yes)
SRCS += s_oss.c
endif
ifeq ($(USE_LIBVCD),yes)
SRCS += s_vcdnav.c
endif
SRCS += mrl.c

SUBDIRS = tvi
ifeq ($(HAVE_STREAMING),yes)
SUBDIRS += freesdp librtsp realrtsp
SRCS += asf_streaming.c asf_mmst_streaming.c cookies.c http.c network.c rtp.c pnm.c s_ftp.c s_udp.c s_rtsp.c s_network.c tcp.c udp.c
endif

CXXSRCS+= stream.cpp cache2.cpp

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
	$(wildcard tvi/*.o) \
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
