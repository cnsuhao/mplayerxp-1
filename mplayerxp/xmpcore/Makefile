include ../mp_config.mak

LIBNAME = libxmpcore.a

SRCS=xmp_core.cpp xmp_aplayer.cpp xmp_vplayer.cpp xmp_vdecoder.cpp xmp_adecoder.cpp
SRCS+=sig_hand.cpp
CSRCS+=mp_aframe.c mp_image.c
OBJS=$(SRCS:.cpp=.o)
COBJS=$(CSRCS:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall
CXXFLAGS  = $(OPXXTFLAGS) -I. -I.. -Wall
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME): $(OBJS) $(COBJS)
	$(AR) r $(LIBNAME) $(OBJS) $(COBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CXXFLAGS) $(SRCS) $(CSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
