include ../mpxp_config.mak

ifeq ($(ENABLE_WIN32LOADER),yes)
LIBNAME=libwin32loader.a
SUBDIRS=dshow dmo
else
LIBNAME=
SUBDIRS=
endif
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done
DO_ALL  = @ for i in $(SUBDIRS); do $(MAKE) -C $$i all || exit; done

# Generated automatically from Makefile.in by configure.
DEFINES=$(WIN32_PATH) -DMPLAYER -D__WINE__

ASRCS=wrapper.S

SRCS= driver.c \
      afl.c \
      vfl.c
ifneq ($(TARGET_OS),WIN32)
SRCS+= ldt_keeper.c \
       pe_image.c \
       module.c \
       ext.c \
       win32.c \
       pe_resource.c \
       resource.c \
       registry.c \
       elfdll.c
endif

OBJS=$(SRCS:.c=.o)
AOBJS=$(ASRCS:.S=.o)

# gcc-3.0 produces buggy code for acmStreamOpen() with
# "-O3 -fomit-frame-pointer" or "-O2 -fomit-frame-pointer
# -finline-functions -frename-registers" (code is OK with sole -O2),
# the bad code accesses parameters via %ebp without setting up a
# propper %ebp first!
# -fno-omit-frame-pointer works around this gcc-3.0 bug.  gcc-2.95.2 is OK.
# Note: -D_FILE_OFFSET_BITS=32 is required to disable using mmap64(),
# as it's broken in glibc 2.1.2 (bad header) and 2.1.3 (bad code)
WARN_FLAGS = -Wmissing-prototypes -Wimplicit-function-declaration
CFLAGS=-I. -I.. $(OPTFLAGS) -D_FILE_OFFSET_BITS=32 $(EXTRA_INC) $(WARN_FLAGS) -fno-omit-frame-pointer
#CFLAGS=-I. -I.. -O $(WARN_FLAGS) -g #-fno-omit-frame-pointer

.PHONY: $(SUBDIRS)

all:	dep $(LIBNAME)

$(SUBDIRS):
	$(DO_ALL)

clean:
	$(DO_MAKE)
	-rm -f *.o *~ $(LIBNAME)

distclean: clean
	$(DO_MAKE)
	-rm -f config.h config.mak configure.log .depend

.c.o: $@
	$(CC) $(CFLAGS) $(DEFINES) -c $<
.S.o: $@
	$(CC) $(CFLAGS) -c $< -o $@
ifeq ($(TARGET_OS),OpenBSD)
	./loader_objfix.sh
endif

$(LIBNAME): $(SUBDIRS) $(OBJS) $(AOBJS)
	$(AR) r $(LIBNAME) $(OBJS) $(AOBJS) \
	$(wildcard dshow/*.o) \
	$(wildcard dmo/*.o)

dep:    .depend

.depend: $(SRCS) $(CXXSRCS)
	$(DO_MAKE)
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

ifneq ($(wildcard .depend),)
include .depend
endif
