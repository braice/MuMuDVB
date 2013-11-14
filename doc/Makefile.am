#We just have a doc target to compile the documentation, no installation is made
ASCIIDOCFLAGS   = -a toc -a numbered
htmldocdir	= html
html_DOC	= $(htmldocdir)/README.html $(htmldocdir)/README_CONF.html $(htmldocdir)/QUICKSTART.html $(htmldocdir)/WEBSERVICES.html $(htmldocdir)/TRANSCODE_EXTERNAL.html $(htmldocdir)/index.html
#DISTCLEANFILES  = $(html_DOC)

doc: $(html_DOC)

$(htmldocdir)/%.html: %.txt
	install -d $(htmldocdir)
	-asciidoc $(ASCIIDOCFLAGS) -o $@ $^
