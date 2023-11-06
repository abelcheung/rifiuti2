# vim: set sw=4 ts=4 noexpandtab :
#
# Make file fragment for rifiuti2 -- maintainer only
# Handle binary distribution for Windows
#

include Makefile

ZIPNAME ?= $(distdir)-win-$(build_cpu)

dist-win: win-pkg-data win-pkg-bin
	cd win-pkg && \
	rm -f $(ZIPNAME).zip && \
	7z a -bd -o$(abs_top_builddir) $(ZIPNAME).zip .

win-pkg-data: win-pkg/README.html

win-pkg-bin: \
	win-pkg/rifiuti.exe \
	win-pkg/rifiuti-vista.exe

win-pkg/README.html: $(top_srcdir)/src/rifiuti.1
	set -e ;\
	tmpfile1=$$(mktemp) ;\
	tmpfile2=$$(mktemp) ;\
	groff -Thtml -mman $< | \
		sed -r 's@<p(.*)>(####CHANGELOG####)</p>@<div\1>\n\2\n</div>@' > $$tmpfile1 ;\
	( sed -e '0,/^##[^#]/d; /^----/,$$d;' $(abs_top_srcdir)/NEWS.md | \
		markdown | sed -r 's@<(/?)h[0-9]>@<\1strong>@g;'; ) > $$tmpfile2 ;\
	( sed -e '/^####CHANGELOG####/,$$d' $$tmpfile1; cat $$tmpfile2; \
		sed -e '0,/^####CHANGELOG####/d' $$tmpfile1 ) > $@ ;\
	rm -f $$tmpfile1 $$tmpfile2

win-pkg/rifiuti.exe: $(top_builddir)/src/rifiuti.exe
	$(MKDIR_P) win-pkg
	strip --strip-unneeded -o $@ $<

win-pkg/rifiuti-vista.exe: $(top_builddir)/src/rifiuti-vista.exe
	$(MKDIR_P) win-pkg
	strip --strip-unneeded -o $@ $<

.PHONY: win-pkg-data win-pkg-bin dist-win
