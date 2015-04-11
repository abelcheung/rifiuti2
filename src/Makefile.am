INCLUDES = -I$(srcdir) \
	   $(GLIB_CFLAGS) \
	   -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = rifiuti rifiuti-vista
man_MANS = rifiuti.1

EXTRA_DIST = rifiuti.1

rifiuti_SOURCES = \
		  rifiuti.c \
		  rifiuti.h \
		  utils.c \
		  utils.h

rifiuti_LDADD = $(GLIB_LIBS)

rifiuti_vista_SOURCES = \
			rifiuti-vista.c \
			rifiuti-vista.h \
			utils.c \
			utils.h

rifiuti_vista_LDADD = $(GLIB_LIBS)

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && $(LN_S) rifiuti.1 rifiuti-vista.1

-include $(top_srcdir)/git.mk
