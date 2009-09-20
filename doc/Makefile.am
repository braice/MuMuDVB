docdir = $(datadir)/doc/@PACKAGE@
man_MANS = mumudvb.1

ASCIIDOCFLAGS   = -a toc -a numbered
DOC_FILES	= README.html README-fr.html README_CONF.html README_CONF-fr.html QUICKSTART.html
SRC_DOC_FILES	= $(DOC_FILES:.html=.txt)
EXAMPLES        = configuration_examples/autoconf1.conf configuration_examples/autoconf_full.conf configuration_examples/example.conf
DISTCLEANFILES  = $(DOC_FILES)
doc_DATA        = $(SRC_DOC_FILES) $(EXAMPLES)

doc: $(DOC_FILES)

%.html: %.txt
	-asciidoc $(ASCIIDOCFLAGS) $^
