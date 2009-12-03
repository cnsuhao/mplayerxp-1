include config.mak

ifeq ($(TARGET_ARCH_X86),yes)
LIBNAME=libloader$(SLIBSUF)
SUBDIRS=dshow dmo
else
LIBNAME=
SUBDIRS=
endif
BINDIR = $(LIBDIR)/$(PROGNAME)/wine
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done

# Generated automatically from Makefile.in by configure.
DEFINES=$(WIN32_PATH) -DMPLAYER -D__WINE__

LIB_OBJECTS= driver.o afl.o vfl.o
ifneq ($(TARGET_OS),WIN32)
LIB_OBJECTS+= ldt_keeper.o pe_image.o module.o ext.o win32.o pe_resource.o \
resource.o registry.o elfdll.o cpudetect.o get_path.o wrapper.o
endif

LD_LIBS=-lc -lm -lpthread
ifeq ($(TARGET_OS),WIN32)
LD_LIBS+=-lkernel32
endif

ifeq ($(TARGET_OS),WIN32)
LD=$(CC)
LD_FLAGS=$(LDFLAGS) --dll --disable-runtime-pseudo-reloc --enable-auto-image-base --gc-sections --sort-common
else
LD_FLAGS=$(LDFLAGS) 
endif

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

all:	$(LIBNAME) $(SUBDIRS) 
ifeq ($(TARGET_ARCH_X86),yes)
	$(DO_MAKE)
endif

clean:
ifeq ($(TARGET_ARCH_X86),yes)
	$(DO_MAKE)
endif
	-rm -f *.o $(LIBNAME)

distclean: clean
ifeq ($(TARGET_ARCH_X86),yes)
	$(DO_MAKE)
endif
	-rm -f config.h config.mak configure.log

.c.o: $@
	$(CC) $(CFLAGS) $(DEFINES) -c $<
.S.o: $@
	$(CC) $(CFLAGS) -c $< -o $@
ifeq ($(TARGET_OS),OpenBSD)
	./loader_objfix.sh
endif

ifeq ($(TARGET_ARCH_X86),yes)
$(LIBNAME): $(LIB_OBJECTS)
	$(CC) --shared $(LD_FLAGS) -o $@ $(LIB_OBJECTS) $(LD_LIBS)
endif

dep:
ifeq ($(TARGET_ARCH_X86),yes)
	echo "dependency not required/supported"
	$(DO_MAKE)
endif
install:
ifeq ($(TARGET_ARCH_X86),yes)
	$(DO_MAKE)
	install -D -m 755 -s -p $(LIBNAME) $(DESTDIR)$(BINDIR)/$(LIBNAME)
endif

uninstall:
	$(DO_MAKE)
ifeq ($(TARGET_ARCH_X86),yes)
	rm -f $(DESTDIR)$(BINDIR)/$(LIBNAME)
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(BINDIR)
endif
