# a BSD compatible 'install' program
INSTALL = install
include ../config.mak
LIBNAME = libMP3$(SLIBSUF)
BINDIR = $(LIBDIR)/$(PROGNAME)/codecs
SRCS = tabinit.c sr1.c layer1.c layer2.c layer3.c dct12.c dct36.c dct64.c decod386.c
OBJS = tabinit.o sr1.o layer1.o layer2.o layer3.o dct12.o dct36.o dct64.o decod386.o
# OBJS = $(SRCS:.c,.s=.o)
ifeq ($(TARGET_ARCH_SGI_MIPS),yes)
OPTFLAGS := $(OPTFLAGS:-O4=-O0)
endif
CFLAGS  = $(OPTFLAGS) $(EXTRA_INC)
ifeq ($(TARGET_ARCH_X86),yes)
CFLAGS += -W -Wall -fPIC
SRCS += dct36_3dnow.c decode_3dnow.c dct64_k7.c
OBJS += dct36_3dnow.o decode_3dnow.o dct64_k7.o
endif
ifeq ($(TARGET_ARCH_X86_64),yes)
CFLAGS += -W -Wall -fPIC
SRCS += decode_3dnow.c dct36_3dnow.c dct64_k7.c
OBJS += decode_3dnow.o dct36_3dnow.o dct64_k7.o
endif

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.s.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(CC) --shared -o $@ $(OBJS)  -lm -lc 

test1:	$(LIBNAME) test.c
	$(CC) $(CFLAGS) test.c ../../mplayerxp/my_profile.c -o test1 -I.. -L. -lMP3 -lm -lc $(EXTRALIBS) -Xlinker -rpath ./

test2:	$(LIBNAME) test2.c
	$(CC) $(CFLAGS) test2.c -o test2 -I.. -L. -lMP3 -lm -lc $(EXTRALIBS) -Xlinker -rpath ./

all:	$(LIBNAME)

install:
	$(INSTALL) -D -m 755 -s -p $(LIBNAME) $(DESTDIR)$(BINDIR)/$(LIBNAME)
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(LIBNAME)
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)
clean:
	rm -f *~ *.o *$(SLIBSUF)

distclean:
	rm -f *~ *.o *$(SLIBSUF) Makefile.bak .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
