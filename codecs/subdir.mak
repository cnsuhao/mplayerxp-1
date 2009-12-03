SRC_DIR := $(SRC_PATH_BARE)/lib$(NAME)
SHLIBDIR=$(CODECDIR)

include $(SUBDIR)../common.mak

LIBVERSION := $(lib$(NAME))ver_mpxp
LIBMAJOR   := $(lib$(NAME))mpxp

ifeq ($(BUILD_STATIC),yes)
all: $(SUBDIR)$(LIBNAME)

install-libs: install-lib$(NAME)-static

$(SUBDIR)$(LIBNAME): $(OBJS)
	rm -f $@
	$(AR) rc $@ $^ $(EXTRAOBJS)
	$(RANLIB) $@
endif

INCINSTDIR := $(INCDIR)/lib$(NAME)

define RULES
ifdef BUILD_SHARED
all: $(SUBDIR)$(SLIBNAME)

install-libs: install-lib$(NAME)-shared

$(SUBDIR)$(SLIBNAME): $(SUBDIR)$(SLIBNAME_WITH_MAJOR)
	cd ./$(SUBDIR) && $(LN_S) $(SLIBNAME_WITH_MAJOR) $(SLIBNAME)

$(SUBDIR)$(SLIBNAME_WITH_MAJOR): $(OBJS)
	$(SLIB_CREATE_DEF_CMD)
	install -d "$(DESTDIR)$(SHLIBDIR)"
	$(CC) $(SHFLAGS) $(FFLDFLAGS) -o $$@ $$(filter-out $(DEP_LIBS),$$^) $(FFEXTRALIBS) $(EXTRAOBJS)
	$(SLIB_EXTRA_CMD)

ifdef SUBDIR
$(SUBDIR)$(SLIBNAME_WITH_MAJOR): $(DEP_LIBS)
endif
endif

#install: istall-lib$(NAME)-shared

install-lib$(NAME)-shared: $(SUBDIR)$(SLIBNAME)
	install -d "$(DESTDIR)$(SHLIBDIR)"
	install -m 755 $(SUBDIR)$(SLIBNAME) "$(DESTDIR)$(SHLIBDIR)/$(SLIBNAME_WITH_VERSION)"
	$(STRIP) "$(DESTDIR)$(SHLIBDIR)/$(SLIBNAME_WITH_VERSION)"
	cd "$(DESTDIR)$(SHLIBDIR)" && \
		$(LN_S) $(SLIBNAME_WITH_VERSION) $(SLIBNAME_WITH_MAJOR)
	cd "$(DESTDIR)$(SHLIBDIR)" && \
		$(LN_S) $(SLIBNAME_WITH_VERSION) $(SLIBNAME)
	$(SLIB_INSTALL_EXTRA_CMD)

install-lib$(NAME)-static: $(SUBDIR)$(LIBNAME)
	install -d "$(LIBDIR)"
	install -m 644 $(SUBDIR)$(LIBNAME) "$(LIBDIR)"
	$(LIB_INSTALL_EXTRA_CMD)

install-headers::
	install -d "$(INCINSTDIR)"
	install -d "$(LIBDIR)/pkgconfig"
	install -m 644 $(addprefix "$(SRC_DIR)"/,$(HEADERS)) "$(INCINSTDIR)"
	install -m 644 $(BUILD_ROOT)/lib$(NAME)/lib$(NAME).pc "$(LIBDIR)/pkgconfig"

uninstall-libs::
	-rm -f "$(DESTDIR)$(SHLIBDIR)/$(SLIBNAME_WITH_MAJOR)" \
	       "$(DESTDIR)$(SHLIBDIR)/$(SLIBNAME)"            \
	       "$(DESTDIR)$(SHLIBDIR)/$(SLIBNAME_WITH_VERSION)"
	-$(SLIB_UNINSTALL_EXTRA_CMD)
	-rm -f "$(LIBDIR)/$(LIBNAME)"

uninstall-headers::
	rm -f $(addprefix "$(INCINSTDIR)/",$(HEADERS))
	rm -f "$(LIBDIR)/pkgconfig/lib$(NAME).pc"
	-rmdir "$(INCDIR)"
endef

$(eval $(RULES))
