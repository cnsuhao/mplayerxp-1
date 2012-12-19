include ../mpxp_config.mak

LIBNAME = libxmpcore.a

SRCS=xmp_context.cpp \
     xmp_core.cpp \
     xmp_aplayer.cpp \
     xmp_vplayer.cpp \
     xmp_vdecoder.cpp \
     xmp_adecoder.cpp

SRCS+=sig_hand.cpp xmp_image.cpp xmp_aframe.cpp
OBJS=$(SRCS:.cpp=.o)

CXXFLAGS  = $(OPTXXFLAGS) -I. -I.. -Wall
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME): $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:   dep $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    .depend

.depend: $(SRCS) $(CXXSRCS)
	$(CXX) -MM $(CXXFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
