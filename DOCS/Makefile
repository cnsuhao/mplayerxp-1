# a BSD compatible 'install' program
INSTALL = install

include config.mak

MANDIR = ${prefix}/man

all:
clean:
distclean:
	rm -f config.mak
install:
	$(INSTALL) -D -c -m 644 mplayerxp.1 $(DESTDIR)$(MANDIR)/man1/$(PROGNAME).1
uninstall:
	rm -f $(MANDIR)/man1/mplayerxp.1
	rmdir -p --ignore-fail-on-non-empty $(DESTDIR)$(MANDIR)/man1
dep:
