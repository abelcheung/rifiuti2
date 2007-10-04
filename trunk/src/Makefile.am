INCLUDES = -I. \
	   $(GLIB_CFLAGS) \
	   -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = rifiuti rifiuti-vista

rifiuti_SOURCES = \
		  rifiuti.c \
		  utils.c

rifiuti_LDADD = $(GLIB_LIBS)

rifiuti_vista_SOURCES = \
			rifiuti-vista.c \
			utils.c

rifiuti_vista_LDADD = $(GLIB_LIBS)


