
include ../mpxp_config.mak

LIBNAME = libpostproc.a
SUBDIRS = libmenu
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

CXXSRCS=af.cpp aflib.cpp vf.cpp  postprocess.cpp swscale.cpp

CXXSRCS+=af_ao3.cpp \
      af_channels.cpp \
      af_crystality.cpp \
      af_delay.cpp \
      af_dyn.cpp \
      af_echo3d.cpp \
      af_equalizer.cpp \
      af_export.cpp \
      af_extrastereo.cpp \
      af_ffenc.cpp \
      af_format.cpp \
      af_hrtf.cpp \
      af_karaoke.cpp \
      af_lp.cpp \
      af_null.cpp \
      af_pan.cpp \
      af_raw.cpp \
      af_resample.cpp \
      af_scaletempo.cpp \
      af_sinesuppress.cpp \
      af_sub.cpp \
      af_surround.cpp \
      af_tools.cpp \
      af_volnorm.cpp \
      af_volume.cpp

CXXSRCS+=vf_1bpp.cpp \
      vf_2xsai.cpp \
      vf_aspect.cpp \
      vf_delogo.cpp \
      vf_denoise3d.cpp \
      vf_dint.cpp \
      vf_down3dright.cpp \
      vf_eq.cpp \
      vf_expand.cpp \
      vf_flip.cpp \
      vf_format.cpp \
      vf_framestep.cpp \
      vf_il.cpp \
      vf_menu.cpp \
      vf_mirror.cpp \
      vf_noise.cpp \
      vf_null.cpp \
      vf_ow.cpp \
      vf_palette.cpp \
      vf_panscan.cpp \
      vf_perspective.cpp \
      vf_pp.cpp \
      vf_raw.cpp \
      vf_rectangle.cpp \
      vf_rgb2bgr.cpp \
      vf_rotate.cpp \
      vf_scale.cpp \
      vf_smartblur.cpp \
      vf_softpulldown.cpp \
      vf_swapuv.cpp \
      vf_test.cpp \
      vf_unsharp.cpp \
      vf_vo2.cpp \
      vf_yuvcsp.cpp \
      vf_yuy2.cpp \
      vf_yvu9.cpp

OBJS=$(SRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cpp=.o)
CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall
CXXFLAGS  = $(OPTFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o

.PHONY: $(SUBDIRS)

all:   dep $(LIBNAME)

$(SUBDIRS):
	$(DO_ALL)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME):  $(SUBDIRS) $(OBJS) $(CXXOBJS)
	$(AR) r $(LIBNAME) $(OBJS) $(CXXOBJS) \
	$(wildcard libmenu/*.o)

clean:
	$(DO_MAKE)
	rm -f *.o *.a *~

distclean:
	$(DO_MAKE)
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    .depend

.depend: $(SRCS) $(CXXSRCS)
	$(DO_MAKE)
	$(CC) -MM $(CFLAGS) $(SRCS)  $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
