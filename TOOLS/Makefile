include ../mplayerxp/mp_config.mak

CFLAGS = -g3 -I..

OBJS = bios2dump$(EXESUF) \
       mem2dump$(EXESUF) \
       movinfo$(EXESUF)

MP_LIBS = ../mplayerxp/libmpdemux/libmpdemux.a
ifeq ($(HAVE_CSS),yes)
MP_LIBS += ../mplayerxp/libmpdemux/librtsp/librtsp.a ../mplayerxp/libmpdemux/realrtsp/librealrtsp.a \
../mplayerxp/libmpdemux/freesdp/libfreesdp.a
endif
MP_LIBS += ../mplayerxp/libmpcodecs/libmpcodecs.a ../mplayerxp/libao2/libao2.a ../mplayerxp/postproc/libpostproc.a \
../mplayerxp/input/libinput.a ../mplayerxp/libvo/libvo.a \
../mplayerxp/osdep/libosdep.a ../mplayerxp/mp_msg.o ../mplayerxp/nls/libnls.a ../mplayerxp/cpudetect.o

COMMON_LIBS = $(MP_LIBS) $(EXTRALIBS) -lm
CFLAGS = $(OPTFLAGS) -I ../mplayerxp -I../mplayerxp/libmpdemux -I../mplayerxp/libvo $(EXTRA_INC)

all: $(OBJS)

fastmemcpybench: fastmemcpybench.c
	$(CC) $(CFLAGS) $< $(COMMON_LIBS) -o fastmem-mmx$(EXESUF)  -DNAME=\"mmx\"      -DHAVE_MMX
	$(CC) $(CFLAGS) $< $(COMMON_LIBS) -o fastmem-k6$(EXESUF)   -DNAME=\"k6\ \"     -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) $< $(COMMON_LIBS) -o fastmem-k7$(EXESUF)   -DNAME=\"k7\ \"     -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) $< $(COMMON_LIBS) -o fastmem-sse$(EXESUF)  -DNAME=\"sse\"      -DHAVE_MMX -DHAVE_SSE -DHAVE_MMX2

ASMOPTOBJS = asmopt.o
#CFLAGS=-E
asmopt$(EXESUF): $(ASMOPTOBJS)
	$(CC) $(CFLAGS) $< $(COMMON_LIBS) -o $@


MMX2COBJS = mmx2c.o
mmx2c$(EXESUF): $(MMX2COBJS)
	$(CC) $(CFLAGS) $< -o $@

#asmopt$(EXESUF): asmopt.c asmopt_template.h
#	$(CC) $(CFLAGS) $< $(COMMON_LIBS) -o asmopt$(EXESUF)

clean distclean:
	rm -f *.o *~ $(OBJS)
	rm -f fastmem-* fastmem2-* fastmemcpybench netstream
	rm -f asmopt$(EXESUF)

.PHONY: all fastmemcpybench realcodecs clean distclean
