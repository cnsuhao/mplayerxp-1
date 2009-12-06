SUBDIRS = codecs loader mplayerxp etc DOCS

DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@ || exit; done

all: $(SUBDIRS)

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

install:
	$(DO_MAKE)

clean:
	$(DO_MAKE)

distclean:
	$(DO_MAKE)
	rm -f config.h config.mak

uninstall:
	$(DO_MAKE)

dep:
	$(DO_MAKE)

