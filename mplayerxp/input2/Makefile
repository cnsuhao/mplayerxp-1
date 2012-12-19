
include ../mpxp_config.mak

LIBNAME = libinput2.a

SRCS=input.cpp joystick.cpp lirc.cpp
OBJS=$(SRCS:.cpp=.o)

CXXFLAGS  = $(OPTXXFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:  dep  $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    .depend

.depend: $(SRCS)
	$(CXX) -MM $(CXXFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
