# a BSD compatible 'install' program
INSTALL = install

include config.mak

all:
clean:
distclean:
	rm -f config.mak configure.log
install:
	$(INSTALL) -D -b -c -m 644 codecs.conf $(DESTDIR)$(CONFDIR)/codecs.conf
	$(INSTALL) -D -b -c -m 644 menu.conf $(DESTDIR)$(CONFDIR)/menu.conf
	$(INSTALL) -D -b -c -m 644 eqbands $(DESTDIR)$(CONFDIR)/eqbands
uninstall:
	rm -f $(CONFDIR)/*
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(CONFDIR)
dep:
