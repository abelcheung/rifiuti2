# vim: set sw=4 ts=4 noexpandtab :

AM_CPPFLAGS = \
	-I$(srcdir)                     \
	$(GLIB_CFLAGS)                  \
	-DLOCALEDIR=\"$(localedir)\"    \
	-DLOCALEDIR_PORTABLE=\"$(LOCALEDIR_PORTABLE)\" \
	-DG_LOG_DOMAIN=\"$(PACKAGE)\"

if STATIC_BUILD
AM_LDFLAGS = -static
endif

bin_PROGRAMS = rifiuti rifiuti-vista
man_MANS = rifiuti.1

EXTRA_DIST = rifiuti.1

COMMON_SOURCES = \
	utils.c \
	utils.h \
	$(NULL)

rifiuti_SOURCES = \
	rifiuti.c \
	rifiuti.h \
	$(COMMON_SOURCES)

rifiuti_LDADD = $(GLIB_LIBS)

rifiuti_vista_SOURCES = \
	rifiuti-vista.c \
	rifiuti-vista.h \
	$(COMMON_SOURCES)

rifiuti_vista_LDADD = $(GLIB_LIBS)

if OS_WINDOWS
WIN_SOURCES = \
	utils-win.c \
	utils-win.h \
	$(NULL)

rifiuti_SOURCES       += $(WIN_SOURCES)
rifiuti_vista_SOURCES += $(WIN_SOURCES)
rifiuti_LDADD         += -lauthz
rifiuti_vista_LDADD   += -lauthz
endif

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && $(LN_S) rifiuti.1 rifiuti-vista.1

uninstall-hook:
	-rm -f $(DESTDIR)$(mandir)/man1/rifiuti-vista.1

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
