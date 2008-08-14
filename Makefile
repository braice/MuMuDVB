IFLAGS	= -g root -o root

ifdef DESTDIR
  MANDIR = $(DESTDIR)/usr/share/man/man1
else
  MANDIR = /usr/share/man/man1
endif

.PHONY: all clean install uninstall install_man

all:
	$(MAKE) -C src
	$(MAKE) -C tools

clean:
	$(MAKE) clean -C src
	$(MAKE) clean -C tools

install:
	$(MAKE) install -C src
	install -d $(DESTDIR)/usr/share/doc/mumudvb
	install -d $(DESTDIR)/usr/share/doc/mumudvb/examples
	install -m 644 doc/README_CONF* $(DESTDIR)/usr/share/doc/mumudvb
	install -m 644 doc/QUICKSTART $(DESTDIR)/usr/share/doc/mumudvb
	install -m 644 README README-fr $(DESTDIR)/usr/share/doc/mumudvb
	install -m 644 doc/conf.example* $(DESTDIR)/usr/share/doc/mumudvb/examples
	mkdir -p $(DESTDIR)/var/run/mumudvb

uninstall:
	$(MAKE) uninstall -C src

install_man:
	install $(IFLAGS) -d $(MANDIR)
	install $(IFLAGS) -m 644 doc/mumudvb.1 $(MANDIR)
	gzip -9 $(MANDIR)/mumudvb.1
