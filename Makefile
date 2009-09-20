IFLAGS	= -g root -o root

.PHONY: all doc clean install uninstall install_man

all:
	$(MAKE) -C src

doc:
	$(MAKE) doc -C doc

clean:
	$(MAKE) clean -C src
	$(MAKE) clean -C doc

install:
	$(MAKE) install -C src
	mkdir -p $(DESTDIR)/var/run/mumudvb

uninstall:
	$(MAKE) uninstall -C src

install_doc:
	$(MAKE) install_doc -C doc

