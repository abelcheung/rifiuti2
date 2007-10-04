INCLUDES = -I$(srcdir) \
	   $(GLIB_CFLAGS) \
	   -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = rifiuti rifiuti-vista

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


