#We just have a doc target to compile the documentation, no installation is made
ASCIIDOCFLAGS   = -a toc -a numbered
htmldocdir	= html
html_DOC	= $(htmldocdir)/README.html $(htmldocdir)/README_CONF.html $(htmldocdir)/QUICKSTART.html $(htmldocdir)/WEBSERVICES.html $(htmldocdir)/TRANSCODE_EXTERNAL.html $(htmldocdir)/FAQ.html $(htmldocdir)/index.html
#DISTCLEANFILES  = $(html_DOC)

doc: $(html_DOC)

$(htmldocdir)/%.html: %.asciidoc
	install -d $(htmldocdir)
	@if command -v asciidoc >/dev/null 2>&1; then \
		asciidoc $(ASCIIDOCFLAGS) -o $@ $^; \
	elif command -v asciidoctor >/dev/null 2>&1; then \
		asciidoctor $(ASCIIDOCFLAGS) -o $@ $^; \
	else \
		echo "Neither asciidoc nor asciidoctor found; cannot build $@"; exit 1; \
	fi
