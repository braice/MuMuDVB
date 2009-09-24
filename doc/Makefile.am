#We just have a doc target to compile the documentation, no installation is made
ASCIIDOCFLAGS   = -a toc -a numbered
htmldocdir	= asciidoc
html_DOC	= $(htmldocdir)/README.html $(htmldocdir)/README-fr.html $(htmldocdir)/README_CONF.html $(htmldocdir)/README_CONF-fr.html $(htmldocdir)/QUICKSTART.html $(htmldocdir)/index.html
DISTCLEANFILES  = $(html_DOC)

doc: $(html_DOC)

$(htmldocdir)/%.html: %.txt
	-asciidoc $(ASCIIDOCFLAGS) -o $@ $^
