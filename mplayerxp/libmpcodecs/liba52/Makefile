include ../../mp_config.mak

CXXSRCS = parse.cpp bitstream.cpp imdct.cpp downmix.cpp bit_allocate.cpp crc.cpp resample.cpp
SRCS    =
OBJS	= $(SRCS:.c=.o)
CXXOBJS	= $(CXXSRCS:.cpp=.o)

CFLAGS  = $(MLIB_INC) $(OPTFLAGS) -W -Wall
CXXFLAGS  = $(MLIB_INC) $(OPTXXFLAGS) -W -Wall

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CC) -c $(CXXFLAGS) -o $@ $<

all:	dep $(OBJS) $(CXXOBJS)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test *.o $(LIBNAME) *~ .depend

dep:    .depend

.depend: $(SRCS) $(CXXSRCS)
	$(CC) -MM $(CFLAGS) $(SRCS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
