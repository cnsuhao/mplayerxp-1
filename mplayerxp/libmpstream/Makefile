
LIBNAME = libmpstream.a

include ../mp_config.mak

DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

CXXSRCS=  s_file.cpp s_lavc.cpp s_null.cpp s_tv.cpp
CXXSRCS+= mrl.cpp stream.cpp url.cpp cache2.cpp
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
CXXSRCS += s_dvdread.cpp
endif
ifeq ($(USE_OSS_AUDIO),yes)
CXXSRCS += s_oss.cpp
endif
ifeq ($(USE_LIBVCD),yes)
CXXSRCS += s_vcdnav.cpp
endif

SUBDIRS = tvi
ifeq ($(HAVE_STREAMING),yes)
SUBDIRS += freesdp librtsp realrtsp
CXXSRCS += asf_streaming.cpp \
           asf_mmst_streaming.cpp \
           cookies.cpp \
           http.cpp \
           network.cpp \
           pnm.cpp \
           rtp.cpp \
           tcp.cpp \
           udp.cpp
CXXSRCS += s_ftp.cpp \
           s_network.cpp \
           s_rtsp.cpp \
           s_udp.cpp
endif

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
	$(AR) r $(LIBNAME) \
	$(wildcard tvi/*.o)
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
