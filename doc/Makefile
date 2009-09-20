ifdef DESTDIR
  MANDIR = $(DESTDIR)/usr/share/man/man1
else
  MANDIR = /usr/share/man/man1
endif

ASCIIDOCFLAGS = -a toc -a numbered

DOC_FILES	= README.html README-fr.html README_CONF.html README_CONF-fr.html QUICKSTART.html
SRC_DOC_FILES	= $(DOC_FILES:.html=.txt)

.PHONY: all clean doc install

all: doc

doc: $(DOC_FILES)

clean:
	-rm -f  $(DOC_FILES)

install_doc:
	install -d $(DESTDIR)/usr/share/doc/mumudvb
	install -d $(DESTDIR)/usr/share/doc/mumudvb/examples
	install -m 644 $(SRC_DOC_FILES) $(DOC_FILES) $(DESTDIR)/usr/share/doc/mumudvb
	install -m 644 configuration_examples/* $(DESTDIR)/usr/share/doc/mumudvb/examples
	install $(IFLAGS) -d $(MANDIR)
	install $(IFLAGS) -m 644 mumudvb.1 $(MANDIR)
	gzip -9 $(MANDIR)/mumudvb.1

%.html: %.txt
	-asciidoc $(ASCIIDOCFLAGS) $^

