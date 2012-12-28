
LIBNAME = libmpconf.a

include ../mpxp_config.mak

DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

CXXSRCS=cfgparser.cpp m_option.cpp m_property.cpp m_struct.cpp
SRCS  =
ifeq ($(ENABLE_WIN32LOADER),yes)
CXXSRCS += codec-cfg.cpp
endif

OBJS	= $(SRCS:.c=.o)
CXXOBJS	= $(CXXSRCS:.cpp=.o)
INCLUDE = -I. -I../
CFLAGS  = $(OPTFLAGS) $(INCLUDE) -W -Wall
CXXFLAGS  = $(OPTXXFLAGS) $(INCLUDE) -W -Wall

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
	$(AR) r $(LIBNAME) $(OBJS) $(CXXOBJS)

clean:
	$(DO_MAKE)
	rm -f *.o *.a *~

distclean:
	$(DO_MAKE)
	rm -f test Makefile.bak *.o *.a *~ .depend

dep:    .depend

.depend: $(SRCS) $(CXXSRCS)
	$(DO_MAKE)
	$(CC) -MM $(CFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
