include ../../mp_config.mak

CXXSRCS = bitstream.cpp downmix.cpp parse.cpp resample.cpp
CXXOBJS	= $(CXXSRCS:.cpp=.o)

CXXFLAGS  = $(MLIB_INC) $(OPTXXFLAGS) -W -Wall

.SUFFIXES: .cpp .o

# .PHONY: all clean

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

all:	dep $(CXXOBJS)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test *.o $(LIBNAME) *~ .depend

dep:    .depend

.depend: $(CXXSRCS)
	$(CXX) -MM $(CXXFLAGS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
