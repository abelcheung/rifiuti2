#
# Make file fragment for rifiuti2 -- maintainer only
# Handle binary distribution for Windows
#

dist-win: win-pkg-data win-pkg/x86 win-pkg/x64
	cd win-pkg && zip -9 -r $(abs_top_builddir)/$(distdir)-win.zip .

win-pkg-data: win-pkg/rifiuti-l10n win-pkg/README.html

win-pkg/README.html: $(top_srcdir)/src/rifiuti.1
	set -e ;\
	tmpfile1=$$(mktemp) ;\
	tmpfile2=$$(mktemp) ;\
	groff -Thtml -mman $< | \
		sed -r '/####CHANGELOG####/ s@(</?)p@\1div@g;' | \
		perl -p -e 's/(####CHANGELOG####)/\n$$1\n/' > $$tmpfile1 ;\
	( sed -e '0,/^##[^#]/d; /^----/,$$d;' $(abs_top_srcdir)/NEWS.md | \
		markdown | sed -e 's/h[0-9]>/strong>/g;'; ) > $$tmpfile2 ;\
	( sed -e '/^####CHANGELOG####/,$$d' $$tmpfile1; cat $$tmpfile2; \
		sed -e '0,/^####CHANGELOG####/d' $$tmpfile1 ) > $@ ;\
	rm -f $$tmpfile1 $$tmpfile2

win-pkg/rifiuti-l10n: $(top_srcdir)/po/$(GETTEXT_PACKAGE).pot
	cd po && $(MAKE) install gnulocaledir=$(abs_top_builddir)/$@
	cp $< $(abs_top_builddir)/$@

win-pkg/x86:
	test "`objdump -f $(top_builddir)/src/rifiuti$(EXEEXT) | \
		awk '/file format/ {print $$NF}'`" = "pei-i386"
	objdump -f $(top_builddir)/src/rifiuti$(EXEEXT) | grep -q "HAS_RELOC"
	$(MKDIR_P) $@
	strip --strip-unneeded -o $@/rifiuti$(EXEEXT) \
		$(top_builddir)/src/rifiuti$(EXEEXT)
	strip --strip-unneeded -o $@/rifiuti-vista$(EXEEXT) \
		$(top_builddir)/src/rifiuti-vista$(EXEEXT)

win-pkg/x64:
	test "`objdump -f $(top_builddir)/src/rifiuti$(EXEEXT) | \
		awk '/file format/ {print $$NF}'`" = "pei-x86-64"
	objdump -f $(top_builddir)/src/rifiuti$(EXEEXT) | grep -q "HAS_RELOC"
	$(MKDIR_P) $@
	strip --strip-unneeded -o $@/rifiuti$(EXEEXT) \
		$(top_builddir)/src/rifiuti$(EXEEXT)
	strip --strip-unneeded -o $@/rifiuti-vista$(EXEEXT) \
		$(top_builddir)/src/rifiuti-vista$(EXEEXT)

.PHONY: win-pkg-data dist-win

