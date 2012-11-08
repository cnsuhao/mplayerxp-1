
include ../mp_config.mak

LIBNAME = libpostproc.a
SUBDIRS = libmenu
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

SRCS=postprocess.c swscale.c
SRCS+=af.c af_ao2.c af_crystality.c af_dummy.c af_delay.c af_channels.c af_format.c af_resample.c af_volume.c af_equalizer.c af_tools.c af_comp.c af_gate.c af_pan.c af_surround.c af_sub.c af_export.c af_volnorm.c af_extrastereo.c af_lp.c af_dyn.c af_echo3d.c af_hrtf.c af_ffenc.c af_raw.c af_karaoke.c af_center.c af_sinesuppress.c af_scaletempo.c
SRCS+=dsp.c
SRCS+=vf.c vf_vo.c vf_expand.c vf_flip.c vf_format.c vf_yuy2.c vf_rgb2bgr.c vf_rotate.c vf_mirror.c vf_palette.c vf_test.c vf_noise.c vf_yvu9.c vf_rectangle.c vf_eq.c vf_dint.c vf_1bpp.c vf_unsharp.c vf_swapuv.c vf_il.c vf_smartblur.c vf_perspective.c vf_down3dright.c vf_denoise3d.c vf_aspect.c  vf_softpulldown.c vf_delogo.c vf_yuvcsp.c vf_pp.c vf_scale.c vf_panscan.c vf_raw.c vf_ow.c vf_2xsai.c vf_framestep.c vf_menu.c
OBJS=$(SRCS:.c=.o)
CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o

.PHONY: $(SUBDIRS)

all:    $(LIBNAME)

$(SUBDIRS):
	$(DO_ALL)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):  $(SUBDIRS) $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS) \
	$(wildcard libmenu/*.o)

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
