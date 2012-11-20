include ../../mp_config.mak

CXXSRCS = RTjpegN.cpp minilzo.cpp nuppelvideo.cpp

CXXOBJS=$(CXXSRCS:.cpp=.o)

CXXFLAGS  = $(OPTXXFLAGS) -I. -I../.. $(EXTRA_INC) -Wall

.SUFFIXES: .cpp .o

all: $(CXXOBJS)

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CXX) -MM $(CXXFLAGS) $(CXXSRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

