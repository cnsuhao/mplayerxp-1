
include ../mp_config.mak

LIBNAME = libosdep.a

SRCS=aclib.c cpudetect.c getch2.c timer-lx.c timer.c shmem.c get_path.c
CXXSRCS=mplib.cpp mp_malloc.cpp
OBJS=$(SRCS:.c=.o)
CXXOBJS=$(CXXSRCS:.cpp=.o)

ifeq ($(TARGET_OS),LINUX_GNU)
ifeq ($(TARGET_ARCH_X86),yes)
SRCS += lrmi.c vbelib.c
endif
ifeq ($(TARGET_ARCH_X86_64),yes)
# asm-x86_64/vm86.h does not exist in the x86_64 architecture
endif
endif


CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall
CXXFLAGS  = $(OPTXXFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(LIBNAME): $(CXXOBJS) $(OBJS)
	$(AR) r $(LIBNAME) $(CXXOBJS) $(OBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(CXXSRCS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
