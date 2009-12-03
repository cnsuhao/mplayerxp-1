INSTALL = install

LIBNAME = libfaad2$(SLIBSUF)
BINDIR = $(LIBDIR)/$(PROGNAME)/codecs

include ../config.mak

SRCS    = bits.c cfft.c common.c decoder.c drc.c error.c filtbank.c hcr.c huffman.c ic_predict.c is.c lt_predict.c mdct.c mp4.c ms.c output.c pns.c pulse.c rvlc.c sbr_dct.c sbr_dec.c sbr_e_nf.c sbr_fbt.c sbr_hfadj.c sbr_hfgen.c sbr_huff.c sbr_qmf.c sbr_syntax.c sbr_tf_grid.c specrec.c ssr.c ssr_fb.c ssr_ipqf.c syntax.c tns.c
OBJS	= $(SRCS:.c=.o)

CFLAGS  = -DHAVE_CONFIG_H -DMAIN_DEC -DSSR_DEC -DLTP_DEC -DLD_DEC -DSCALABLE_DEC -DSBR_DEC -I. $(OPTFLAGS)
ifneq ($(TARGET_ARCH_X86),yes)
CFLAGS += -fPIC
endif

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(CC) --shared -o $@ $(OBJS) -lm -lc

all:	$(LIBNAME)

install:
	$(INSTALL) -D -m 755 -s -p $(LIBNAME) $(DESTDIR)$(BINDIR)/$(LIBNAME)
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(LIBNAME)
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test *.o *.a *~ $(LIBNAME) .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
