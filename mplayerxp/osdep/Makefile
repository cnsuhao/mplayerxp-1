
include ../mp_config.mak

LIBNAME = libosdep.a

SRCS=aclib.c cpudetect.c getch2.c timer-lx.c timer.c shmem.c get_path.c my_malloc.c my_profile.c
OBJS=$(SRCS:.c=.o)

ifeq ($(TARGET_OS),LINUX_GNU)
ifeq ($(TARGET_ARCH_X86),yes)
SRCS += lrmi.c vbelib.c
endif
ifeq ($(TARGET_ARCH_X86_64),yes)
# asm-x86_64/vm86.h does not exist in the x86_64 architecture
endif
endif


CFLAGS  = $(OPTFLAGS) -I. -I.. -Wall

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
