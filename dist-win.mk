# Make file fragment for rifiuti2 -- maintainer only
# Handle binary distribution for Windows
#

include Makefile

ZIPNAME ?= $(distdir)-win-$(build_cpu)

dist-win: win-pkg-data win-pkg-bin
	cd win-pkg && \
	rm -f $(ZIPNAME).zip && \
	7z a -bd -o$(abs_top_builddir) $(ZIPNAME).zip .

win-pkg-data:
	$(MKDIR_P) win-pkg
	cp -r $(top_srcdir)/docs win-pkg/
	cp $(top_srcdir)/LICENSE win-pkg/docs/

win-pkg-bin: \
	win-pkg/rifiuti.exe \
	win-pkg/rifiuti-vista.exe

win-pkg/rifiuti.exe: $(top_builddir)/src/rifiuti.exe
	strip --strip-unneeded -o $@ $<

win-pkg/rifiuti-vista.exe: $(top_builddir)/src/rifiuti-vista.exe
	strip --strip-unneeded -o $@ $<

.PHONY: win-pkg-data win-pkg-bin dist-win
