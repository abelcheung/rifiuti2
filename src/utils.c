/* vim: set sw=4 ts=4 noexpandtab : */
/*
 * Copyright (C) 2007-2019 Abel Cheung.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#if HAVE_SETLOCALE
#include <locale.h>
#endif
#include "utils.h"
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#  include <sys/timeb.h>
#  include "utils-win.h"
#endif

/* These aren't intended for public */
#define DECL_OPT_CALLBACK(func)          \
gboolean func (const gchar   *opt_name,  \
               const gchar   *value,     \
               gpointer       data,      \
               GError       **err)

static DECL_OPT_CALLBACK(check_legacy_encoding);
static DECL_OPT_CALLBACK(set_output_path);
static DECL_OPT_CALLBACK(option_deprecated);
static DECL_OPT_CALLBACK(process_delim);

/* WARNING: MUST match order of _os_guess enum */
static char *os_strings[] = {
	"Windows 95",
	"Windows NT 4.0",
	"Windows 98 / 98 SE",
	"Windows Me",
	"Windows 2000",
	"Windows XP / 2003",
	"Windows 2000 / XP / 2003",
	"Windows Vista - 8.1",
	"Windows 10"
};

static int          output_format      = OUTPUT_CSV;
static gboolean     no_heading         = FALSE;
static gboolean     use_localtime      = FALSE;
static gboolean     xml_output         = FALSE;
       char        *delim              = NULL;
       char        *legacy_encoding    = NULL; /*!< INFO2 only, or upon request */
       char        *output_loc         = NULL;
       char        *tmppath            = NULL; /*!< used iff output_loc is defined */
       char       **fileargs           = NULL;
       FILE        *out_fh             = NULL; /*!< unused for Windows console */

/*! These options are only effective for tab delimited mode output */
static const GOptionEntry text_options[] = {
	{
		"delimiter", 't', 0,
		G_OPTION_ARG_CALLBACK, process_delim,
		N_("String to use as delimiter (TAB by default)"), N_("STRING")
	},
	{
		"no-heading", 'n', 0,
		G_OPTION_ARG_NONE, &no_heading,
		N_("Don't show column header and metadata"), NULL
	},
	{
		"always-utf8", '8', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
		G_OPTION_ARG_CALLBACK, option_deprecated,
		N_("(This option is deprecated)"), NULL
	},
	{NULL}
};

static const GOptionEntry main_options[] = {
	{
		"output", 'o', 0,
		G_OPTION_ARG_CALLBACK, set_output_path,
		N_("Write output to FILE"), N_("FILE")
	},
	{
		"xml", 'x', 0,
		G_OPTION_ARG_NONE, &xml_output,
		N_("Output in XML format instead of tab-delimited values"), NULL
	},
	{
		"localtime", 'z', 0,
		G_OPTION_ARG_NONE, &use_localtime,
		N_("Present deletion time in time zone of local system (default is UTC)"),
		NULL
	},
	{
		"version", 'v', G_OPTION_FLAG_NO_ARG,
		G_OPTION_ARG_CALLBACK, (GOptionArgFunc) print_version_and_exit,
		N_("Print version information and exit"), NULL
	},
	{
		G_OPTION_REMAINING, 0, 0,
		G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
		N_("INFO2 file name"), NULL
	},
	{NULL}
};

/*! Appended to main option group if program is INFO2 reader */
const GOptionEntry rbinfile_options[] = {
	{
		"legacy-filename", 'l', 0,
		G_OPTION_ARG_CALLBACK, check_legacy_encoding,
		N_("Show legacy (8.3) path if available and specify its CODEPAGE"),
		N_("CODEPAGE")
	},
	{NULL}
};

size_t
ucs2_strnlen (const gunichar2 *str, size_t max_sz)
{
#ifdef G_OS_WIN32

	return wcsnlen_s ((const wchar_t *) str, max_sz);

#else

	size_t i;

	if (str == NULL)
		return 0;

	for (i=0; (i<max_sz) && str[i]; i++) {}
	return i;

#endif
}

static void
_advance_char (size_t       sz,
               gchar      **in_str,
               gsize       *read_bytes,
               gchar      **out_str,
               gsize       *write_bytes,
               const char  *tmpl)
{
	gchar *repl;

	switch (sz) {
		case 1:
		{
			unsigned char c = *(unsigned char *) (*in_str);
			repl = g_strdup_printf (tmpl, c);
		}
			break;

		case 2:
		{
			gunichar2 c = *(gunichar2 *) (*in_str);
			repl = g_strdup_printf (tmpl, c);
		}
			break;

		default:
			g_assert_not_reached();
	}

	(*in_str) += sz;
	if (read_bytes != NULL)
		(*read_bytes) -= sz;

	*out_str = g_stpcpy (*out_str, (const char *) repl);
	if (write_bytes != NULL)
		*write_bytes -= strlen (repl);

	g_free (repl);
	return;
}

/*! Last argument is there to avoid recomputing */
static char *
_filter_printable_char (const char *str,
                        const char *tmpl,
                        size_t      out_ch_width)
{
	char     *p, *np;
	gunichar  c;
	GString  *s;

	s = g_string_sized_new (strlen (str) * 2);
	p = (char *) str;
	while (*p)
	{
		c  = g_utf8_get_char  (p);
		np = g_utf8_next_char (p);

		/*
		 * ASCII space is the norm (e.g. Program Files), but
		 * all other kinds of spaces are rare, so escape them too
		 */
		if (g_unichar_isgraph (c) || (c == 0x20))
			s = g_string_append_len (s, p, (gssize) (np - p));
		else
			g_string_append_printf (s, tmpl, c);

		p = np;
	}

	return g_string_free (s, FALSE);
}

/*!
 * Converts a Windows path in specified legacy encoding or unicode
 * path into UTF-8 encoded version. When encoding error arises,
 * it attempts to be robust and substitute concerned bytes or
 * unicode codepoints with escaped ones specified by printf-style
 * template. This routine is not for generic charset conversion.
 *
 * 1. Caller is responsible to only supply non-stateful encoding
 * meant to be used as Windows code page, or use NULL to represent
 * UTF-16LE (the Windows unicode path encoding). Never supply
 * any unicode encoding directly.
 *
 * 2. Caller is responsible for using correct printf template
 * for desired data type, no check is done here.
 */
char *
conv_path_to_utf8_with_tmpl (const char *path,
                             const char *from_enc,
                             const char *tmpl,
                             size_t     *read,
                             r2status   *st)
{
	char *u8_path, *i_ptr, *o_ptr, *result = NULL;
	gsize len, r_total, rbyte, wbyte, status, in_ch_width, out_ch_width;
	GIConv conv;

	/* for UTF-16, first byte of str can be null */
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail ((from_enc == NULL) || (*from_enc != '\0'), NULL);
	g_return_val_if_fail ((    tmpl != NULL) && (    *tmpl != '\0'), NULL);

	/* try the template */
	{
		char *s = g_strdup_printf (tmpl, from_enc ? 0xFF : 0xFFFF);
		/* UTF-8 character occupies at most 6 bytes */
		out_ch_width = MAX (strlen(s), 6);
		g_free (s);
	}

	if (from_enc != NULL) {
		in_ch_width = sizeof (char);
		len = strnlen (path, WIN_PATH_MAX);
	} else {
		in_ch_width = sizeof (gunichar2);
		len = ucs2_strnlen ((const gunichar2 *)path, WIN_PATH_MAX);
	}

	if (! len)
		return NULL;

	rbyte   = len *  in_ch_width;
	wbyte   = len * out_ch_width;
	u8_path = g_malloc0 (wbyte);

	r_total = rbyte;
	i_ptr   = (char *) path;
	o_ptr   = u8_path;

	/* Shouldn't fail, from_enc already tested upon start of prog */
	conv = g_iconv_open ("UTF-8", from_enc ? from_enc : "UTF-16LE");

	g_debug ("Initial: read=%" G_GSIZE_FORMAT ", write=%" G_GSIZE_FORMAT,
			rbyte, wbyte);

	/* Pass 1: Convert whole string to UTF-8, all illegal seq become escaped hex */
	while (TRUE)
	{
		int e;

		if (*i_ptr == '\0') {
			if (from_enc   != NULL) break;
			if (*(i_ptr+1) == '\0') break; /* utf-16: check "\0\0" */
		}

		status = g_iconv (conv, &i_ptr, &rbyte, &o_ptr, &wbyte);
		e = errno;

		if ( status != (gsize) -1 ) break;

		g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
			", stt=%" G_GSIZE_FORMAT " (%s) str=%s",
			rbyte, wbyte, status, strerror(e), u8_path);

		/* XXX Should I consider the possibility of odd bytes for EINVAL? */
		switch (e) {
			case EILSEQ:
			case EINVAL:
				_advance_char (in_ch_width, &i_ptr, &rbyte, &o_ptr, &wbyte, tmpl);
				/* reset state, hopefully Windows don't use stateful encoding at all */
				g_iconv (conv, NULL, NULL, &o_ptr, &wbyte);
				*st = R2_ERR_USER_ENCODING;
				break;
			case E2BIG:
				/* Should have already allocated enough buffer. Let it KABOOM! otherwise. */
				g_assert_not_reached();
		}
	}

	g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
		", stt=%" G_GSIZE_FORMAT ", str=%s", rbyte, wbyte, status, u8_path);

	g_iconv_close (conv);

	if (read != NULL)
		*read = r_total - rbyte;

	/* Pass 2: Convert all ctrl characters (and some more) to hex */
	if (g_utf8_validate (u8_path, -1, NULL))
		result = _filter_printable_char (u8_path, tmpl, out_ch_width);
	else {
		g_critical (_("Converted path failed UTF-8 validation"));
		*st = R2_ERR_INTERNAL;
	}

	g_free (u8_path);

	return result;
}


static GString *
get_datetime_str (struct tm *tm)
{
	GString         *output;
	size_t           len;

	output = g_string_sized_new (30);  /* enough for appending numeric timezone */
	len = strftime (output->str, output->allocated_len, "%Y-%m-%d %H:%M:%S", tm);
	if ( !len )
	{
		g_string_free (output, TRUE);
		return NULL;
	}
	output->len = len;
	return output;
}

/* Return full name of current timezone */
static const char *
get_timezone_name (struct tm *tm)
{
	static char buf[100];   /* Don't know theorectical max size, so be generous */

	if (tm == NULL)
		return _("Coordinated Universal Time (UTC)");
	if ( 0 == strftime (buf, sizeof(buf), "%Z", tm) )
		return _("(Failed to retrieve timezone name)");
	return (const char *) (&buf);
}

/*! Return ISO8601 numeric timezone, like "+0400" */
static const char *
get_timezone_numeric (struct tm *tm)
{
	static char buf[10];

	if (tm == NULL)
		return "+0000";  /* ISO8601 forbids -0000 */

	/*
	 * Turns out strftime is not so cross-platform, Windows one supports far
	 * less format strings than that defined in Single Unix Specification.
	 * However, GDateTime is not available until 2.26, so bite the bullet.
	 */
#ifdef G_OS_WIN32
	struct _timeb timeb;
	_ftime (&timeb);
	/*
	 * 1. timezone value is in opposite sign of what people expect
	 * 2. it doesn't account for DST.
	 * 3. tm.tm_isdst is merely a flag and not indication on difference of
	 *    hours between DST and standard time. But there is no way to
	 *    override timezone in C library other than $TZ, and it always use
	 *    US rule, so again, just give up and use the value
	 */
	int offset = MAX(tm->tm_isdst, 0) * 60 - timeb.timezone;
	g_snprintf (buf, sizeof(buf), "%+.2i%.2i", offset / 60, abs(offset) % 60);

#else /* !def G_OS_WIN32 */
	size_t len = strftime (buf, sizeof(buf), "%z", tm);
	if ( !len )
		return "+????";
#endif
	return (const char *) (&buf);
}

/*! Return ISO 8601 formatted time with timezone */
static GString *
get_iso8601_datetime_str (struct tm *tm)
{
	GString         *output;

	if ( ( output = get_datetime_str (tm) ) == NULL )
		return NULL;

	output->str[10] = 'T';
	if ( !use_localtime )
		return g_string_append_c (output, 'Z');

	return g_string_append (output, get_timezone_numeric(tm));
}

void
rifiuti_init (const char *progpath)
{
	if (NULL != g_getenv ("RIFIUTI_DEBUG"))
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			my_debug_handler, NULL);

	setlocale (LC_ALL, "");

#ifdef G_OS_WIN32
	{
		/*
		 * Setting GETTEXT_MUI is not enough. Though it successfully
		 * pick up user default locale under Windows, glib internally
		 * only considers g_win32_getlocale() for decision making,
		 * which in turn only considers thread locale.
		 * So we need to override g_win32_getlocale() result. And
		 * overriding that with LC_* would render GETTEXT_MUI useless.
		 */
		/* _putenv_s ("GETTEXT_MUI", "1"); */
		char *loc = get_win32_locale();

		if (0 == _putenv_s ("LC_MESSAGES", loc)) {
			g_debug ("(Windows) Use LC_MESSAGES = %s", loc);
		} else {
			g_warning ("Failed setting LC_MESSAGES variable, "
				"move on as if no translation is used.");
		}
		g_free (loc);
	}
#endif

	{
		/* searching current dir is more useful on Windows */
		char *d = g_path_get_dirname (progpath);
		char *p = g_build_filename (d, LOCALEDIR_PORTABLE, NULL);

		if (g_file_test (p, G_FILE_TEST_IS_DIR))
		{
			g_debug ("Portable LOCALEDIR = %s", p);
			bindtextdomain (PACKAGE, p);
		}
		else
			bindtextdomain (PACKAGE, LOCALEDIR);

		g_free (p);
		g_free (d);
	}

	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);
}

void
rifiuti_setup_opt_ctx (GOptionContext **context,
                       rbin_type        type)
{
	char         *bug_report_str;
	GOptionGroup *group;

	g_option_context_set_translation_domain (*context, PACKAGE);

	bug_report_str = g_strdup_printf (
		/* TRANSLATOR COMMENT: argument is bug report webpage */
		_("Report bugs to %s"), PACKAGE_BUGREPORT);
	g_option_context_set_description (*context, bug_report_str);
	g_free (bug_report_str);

	/* main group */
	group = g_option_group_new (NULL, NULL, NULL, NULL, NULL);

	g_option_group_add_entries (group, main_options);
	switch (type)
	{
		case RECYCLE_BIN_TYPE_FILE:
			g_option_group_add_entries (group, rbinfile_options);
			break;
		default: break;
		/* There will be option for recycle bin dir later */
	}

	g_option_group_set_translation_domain (group, PACKAGE);
	g_option_context_set_main_group (*context, group);

	/* text group */
	group = g_option_group_new ("text",
		N_("Plain text output options:"),
		N_("Show plain text output options"), NULL, NULL);

	g_option_group_add_entries (group, text_options);
	g_option_group_set_translation_domain (group, PACKAGE);
	g_option_context_add_group (*context, group);

	g_option_context_set_help_enabled (*context, TRUE);
}

gboolean
check_legacy_encoding (const gchar *opt_name,
                       const gchar *enc,
                       gpointer     data,
                       GError     **err)
{
	char           *s;
	gint            e;
	gboolean        ret      = FALSE;
	static gboolean seen     = FALSE;
	GError         *conv_err = NULL;

	if (seen)
	{
		g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
			_("Multiple encoding options disallowed."));
		return FALSE;
	}
	seen = TRUE;

	if ( *enc == '\0' )
	{
		g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
			_("Empty encoding option disallowed."));
		return FALSE;
	}

	s = g_convert ("C:\\", -1, "UTF-8", enc, NULL, NULL, &conv_err);

	if (conv_err == NULL)
	{
		if (strcmp ("C:\\", s) != 0) /* e.g. EBCDIC based code pages */
		{
			g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
				_("'%s' can't possibly be a code page or compatible "
				"encoding used by localized Windows."), enc);
		} else {
			legacy_encoding = g_strdup (enc);
			ret = TRUE;
		}
		goto done_check_encoding;
	}

	e = conv_err->code;
	g_clear_error (&conv_err);

	switch (e)
	{
		case G_CONVERT_ERROR_NO_CONVERSION:

			g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
				_("'%s' encoding is not supported by glib library "
				"on this system.  If iconv program is present on "
				"system, use 'iconv -l' for a list of possible "
				"alternatives; otherwise check out following site for "
				"a list of probable encodings to use:\n\n\t%s"), enc,
#ifdef G_OS_WIN32
				"https://github.com/win-iconv/win-iconv/blob/master/win_iconv.c"
#else
				"https://www.gnu.org/software/libiconv/"
#endif
			);
			break;

		/* Encodings not ASCII compatible can't possibly be ANSI/OEM code pages */
		case G_CONVERT_ERROR_ILLEGAL_SEQUENCE:
		case G_CONVERT_ERROR_PARTIAL_INPUT:

			g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
				_("'%s' can't possibly be a code page or compatible "
				"encoding used by localized Windows."), enc);
			break;

		default:
			g_assert_not_reached ();
	}

done_check_encoding:

	g_free (s);
	return ret;
}


gboolean
set_output_path (const gchar *opt_name,
                 const gchar *value,
                 gpointer     data,
                 GError     **err)
{
	static gboolean seen     = FALSE;

	if (seen)
	{
		g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
			_("Multiple output destinations disallowed."));
		return FALSE;
	}
	seen = TRUE;

	if ( *value == '\0' )
	{
		g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
			_("Empty output filename disallowed."));
		return FALSE;
	}

	if (g_file_test (value, G_FILE_TEST_EXISTS)) {
		g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
			_("Output destinations already exists."));
		return FALSE;
	}

	output_loc = g_strdup (value);
	return TRUE;
}

static gboolean
option_deprecated (const gchar *opt_name,
                   const gchar *unused,
                   gpointer     data,
                   GError     **err)
{
	g_printerr (_("NOTE: Option '%s' is deprecated and ignored."), opt_name);
	g_printerr ("\n");
	return TRUE;
}


r2status
rifiuti_parse_opt_ctx (GOptionContext **context,
                       int             *argc,
                       char          ***argv)
{
	GError   *err = NULL;
	gboolean  ret;

	/* FIXME probably should do GUI help after option parsing is done,
	   g_set_prgname() is called there */

	/* Must be done before parsing arguments since argc will be modified later */
	if (*argc <= 1)
	{
#ifdef G_OS_WIN32
		g_set_print_handler (gui_message);
#endif
		char *help_msg = g_option_context_get_help (*context, FALSE, NULL);
		g_print ("%s", help_msg);
		g_free (help_msg);

		g_option_context_free (*context);
		exit (EXIT_SUCCESS);
	}

#if GLIB_CHECK_VERSION (2, 40, 0)
	{
		char **args;

#  ifdef G_OS_WIN32
		args = g_win32_get_command_line ();
#  else
		args = g_strdupv (*argv);
#  endif
		ret = g_option_context_parse_strv (*context, &args, &err);
		g_strfreev (args);
	}
#else /* glib < 2.40 */
	ret = g_option_context_parse (*context, argc, argv, &err);
#endif

	g_option_context_free (*context);

	if ( !ret )
	{
		g_printerr (_("Error parsing options: %s"), err->message);
		g_printerr ("\n");
		g_error_free (err);
		return R2_ERR_ARG;
	}

	/* Some fallback values after successful option parsing... */
	if (xml_output)
	{
		output_format = OUTPUT_XML;
		if (no_heading || (delim != NULL))
		{
			g_printerr (_("Plain text format options can not be used in XML mode."));
			g_printerr ("\n");
			return R2_ERR_ARG;
		}
	}

	if (delim == NULL)
		delim = g_strdup ("\t");

	return EXIT_SUCCESS;
}


time_t
win_filetime_to_epoch (uint64_t win_filetime)
{
	uint64_t epoch;

	g_debug ("%s(): FileTime = %" G_GUINT64_FORMAT, __func__, win_filetime);

	/* Let's assume we don't need millisecond resolution time for now */
	epoch = (win_filetime - 116444736000000000LL) / 10000000;

	/* Let's assume this program won't survive till 22th century */
	return (time_t) (epoch & 0xFFFFFFFF);
}

/*!
 * Wrapper of g_utf16_to_utf8 for big endian system.
 * Always assume string is nul-terminated. (Unused now?)
 */
char *
utf16le_to_utf8 (const gunichar2   *str,
                 glong              len,
                 glong             *items_read,
                 glong             *items_written,
                 GError           **error)
{
#if ((G_BYTE_ORDER) == (G_LITTLE_ENDIAN))
	return g_utf16_to_utf8 (str, -1, items_read, items_written, error);
#else

	gunichar2 *buf;
	char *ret;

	/* should be guaranteed to succeed */
	buf = (gunichar2 *) g_convert ((const char *) str, len * 2, "UTF-16BE",
	                               "UTF-16LE", NULL, NULL, NULL);
	ret = g_utf16_to_utf8 (buf, -1, items_read, items_written, error);
	g_free (buf);
	return ret;
#endif
}

/*!
 * single/double quotes and backslashes have already been
 * quoted / unquoted when parsing arguments. We need to
 * interpret \\r, \\n etc separately
 */
static char *
_filter_escapes (const char *str)
{
	GString *result, *debug_str;
	char *i = (char *) str;

	g_return_val_if_fail ( (str != NULL) && (*str != '\0'), NULL);

	result = g_string_new (NULL);
	do
	{
		if ( *i != '\\' )
		{
			result = g_string_append_c (result, *i);
			continue;
		}

		switch ( *(++i) )
		{
		  case 'r':
			result = g_string_append_c (result, '\r'); break;
		  case 'n':
			result = g_string_append_c (result, '\n'); break;
		  case 't':
			result = g_string_append_c (result, '\t'); break;
		  case 'e':
			result = g_string_append_c (result, '\x1B'); break;
		  default:
			result = g_string_append_c (result, '\\'); i--;
		}
	}
	while ( *(++i) );

	debug_str = g_string_new ("filtered delimiter = ");
	i = result->str;
	do
	{
		if ( *i >= 0x20 && *i <= 0x7E )  /* problem during linking with g_ascii_isprint */
			debug_str = g_string_append_c (debug_str, *i);
		else
			g_string_append_printf (debug_str, "\\x%02X", *(unsigned char *) i);
	}
	while ( *(++i) );
	g_debug ("%s", debug_str->str);
	g_string_free (debug_str, TRUE);
	return g_string_free (result, FALSE);
}


static gboolean
process_delim (const gchar *opt_name,
               const gchar *value,
               gpointer     data,
               GError     **err)
{
	static gboolean seen = FALSE;

	if (seen)
	{
		g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
			_("Multiple delimiter options disallowed."));
		return FALSE;
	}
	seen = TRUE;

	delim = (*value) ? _filter_escapes (value) : g_strdup ("");

	return TRUE;
}


void
my_debug_handler (const char     *log_domain,
                  GLogLevelFlags  log_level,
                  const char     *message,
                  gpointer        data)
{
	g_printerr ("DEBUG: %s\n", message);
}

static r2status
_get_tempfile (void)
{
	int     fd, e = 0;
	FILE   *h;
	char   *t;

	/* segfaults if string is pre-allocated in stack */
	t = g_strdup ("rifiuti-XXXXXX");

	if ( -1 == ( fd = g_mkstemp (t) ) ) {
		e = errno;
		goto tempfile_fail;
	}

	h = fdopen (fd, "wb");
	if (h == NULL) {
		e = errno;
		close (fd);
		goto tempfile_fail;
	}

	out_fh   = h;
	tmppath  = t;
	return EXIT_SUCCESS;

  tempfile_fail:

	g_printerr (_("Error opening temp file for writing: %s"),
		g_strerror (e));
	g_printerr ("\n");
	return R2_ERR_OPEN_FILE;
}

/*! Scan folder and add all "$Ixxxxxx.xxx" to filelist for parsing */
static gboolean
_populate_index_file_list (GSList     **list,
                          const char  *path)
{
	GDir           *dir;
	const char     *direntry;
	char           *fname;
	GPatternSpec   *pattern1, *pattern2;
	GError         *error = NULL;

	/*
	 * g_dir_open returns cryptic error message or even succeeds on Windows,
	 * when in fact the directory content is inaccessible.
	 */
#ifdef G_OS_WIN32
	if ( !can_list_win32_folder (path) )
		return FALSE;
#endif

	if (NULL == (dir = g_dir_open (path, 0, &error)))
	{
		g_printerr (_("Error opening directory '%s': %s"), path, error->message);
		g_printerr ("\n");
		g_clear_error (&error);
		return FALSE;
	}

	pattern1 = g_pattern_spec_new ("$I??????.*");
	pattern2 = g_pattern_spec_new ("$I??????");

	while ((direntry = g_dir_read_name (dir)) != NULL)
	{
		if (!g_pattern_match_string (pattern1, direntry) &&
		    !g_pattern_match_string (pattern2, direntry))
			continue;
		fname = g_build_filename (path, direntry, NULL);
		*list = g_slist_prepend (*list, fname);
	}

	g_dir_close (dir);

	g_pattern_spec_free (pattern1);
	g_pattern_spec_free (pattern2);

	return TRUE;
}


/*! Search for desktop.ini in folder for hint of recycle bin */
static gboolean
found_desktop_ini (const char *path)
{
	char *filename, *content, *found;

	filename = g_build_filename (path, "desktop.ini", NULL);
	if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
		goto desktop_ini_error;

	/* assume desktop.ini is ASCII and not something spurious */
	if (!g_file_get_contents (filename, &content, NULL, NULL))
		goto desktop_ini_error;

	/* Don't bother parsing, we don't use the content at all */
	found = strstr (content, RECYCLE_BIN_CLSID);
	g_free (content);
	g_free (filename);
	return (found != NULL);

	desktop_ini_error:
	g_free (filename);
	return FALSE;
}


/*! Add potentially valid file(s) to list */
int
check_file_args (const char  *path,
                 GSList     **list,
                 rbin_type    type)
{
	g_debug ("Start basic file checking...");

	g_return_val_if_fail ( (path != NULL) && (list != NULL), R2_ERR_INTERNAL );

	if ( !g_file_test (path, G_FILE_TEST_EXISTS) )
	{
		g_printerr (_("'%s' does not exist."), path);
		g_printerr ("\n");
		return R2_ERR_OPEN_FILE;
	}
	else if ( (type == RECYCLE_BIN_TYPE_DIR) &&
		g_file_test (path, G_FILE_TEST_IS_DIR) )
	{
		if ( ! _populate_index_file_list (list, path) )
			return R2_ERR_OPEN_FILE;
		/*
		 * last ditch effort: search for desktop.ini. Just print empty content
		 * representing empty recycle bin if found.
		 */
		if ( !*list && !found_desktop_ini (path) )
		{
			g_printerr (_("No files with name pattern '%s' "
				"are found in directory."), "$Ixxxxxx.*");
			g_printerr ("\n");
			return R2_ERR_OPEN_FILE;
		}
	}
	else if ( g_file_test (path, G_FILE_TEST_IS_REGULAR) )
		*list = g_slist_prepend ( *list, g_strdup (path) );
	else
	{
		g_printerr ( (type == RECYCLE_BIN_TYPE_DIR) ?
			_("'%s' is not a normal file or directory.") :
			_("'%s' is not a normal file."), path);
		g_printerr ("\n");
		return R2_ERR_OPEN_FILE;
	}
	return EXIT_SUCCESS;
}


static gboolean
_local_printf (const char *format, ...)
{
	va_list        args;
	char          *str;

	g_return_val_if_fail (format != NULL, FALSE);

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	if ( !g_utf8_validate (str, -1, NULL)) {
		g_critical (_("Supplied format or arguments not in UTF-8 encoding"));
		g_free (str);
		return FALSE;
	}

#ifdef G_OS_WIN32
	/*
	 * Use Windows API only if:
	 * 1. On Windows console
	 * 2. Output is not piped nor redirected
	 * See init_wincon_handle().
	 */
	if (out_fh == NULL)
	{
		GError  *err  = NULL;
		wchar_t *wstr = g_utf8_to_utf16 (str, -1, NULL, NULL, &err);

		if (err != NULL) {
			g_critical (_("Error converting output from UTF-8 to UTF-16: %s"), err->message);
			g_clear_error (&err);
			wstr = g_utf8_to_utf16 ("(Original message failed to be displayed in UTF-16)",
				-1, NULL, NULL, NULL);
		}

		puts_wincon (wstr);
		g_free (wstr);
	}
	else
#endif
		fputs (str, out_fh);

	g_free (str);
	return TRUE;
}


r2status
prepare_output_handle (void)
{
	r2status s = EXIT_SUCCESS;

	if (output_loc)
		s = _get_tempfile ();
	else
	{
#ifdef G_OS_WIN32
		if (!init_wincon_handle())
#endif
			out_fh = stdout;
	}
	return s;
}

void
print_header (metarecord  meta)
{
	char             *rbin_path, *ver_string;
	const char       *tz_name, *tz_numeric;
	time_t            t;
	struct tm        *tm;

	if (no_heading) return;

	g_return_if_fail (meta.filename != NULL);

	g_debug ("Entering %s()", __func__);

	rbin_path = g_filename_display_name (meta.filename);

	switch (output_format)
	{
		case OUTPUT_CSV:
		{
			GString *s = g_string_new (NULL);
			char *outstr;

			g_string_printf (s, _("Recycle bin path: '%s'"), rbin_path);
			s = g_string_append_c (s, '\n');

			switch (meta.version)
			{
			  case VERSION_NOT_FOUND:
				/* TRANSLATOR COMMENT: Error when trying to determine recycle bin version */
				ver_string = g_strdup (_("??? (empty folder)"));
				break;
			  case VERSION_INCONSISTENT:
				/* TRANSLATOR COMMENT: Error when trying to determine recycle bin version */
				ver_string = g_strdup (_("??? (version inconsistent)"));
				break;
			default:
				ver_string = g_strdup_printf ("%" G_GUINT64_FORMAT, meta.version);
			}
			g_string_append_printf (s, _("Version: %s"), ver_string);
			g_free (ver_string);
			s = g_string_append_c (s, '\n');

			if (meta.os_guess == OS_GUESS_UNKNOWN)
				s = g_string_append (s, _("OS detection failed"));
			else
				g_string_append_printf (s, _("OS Guess: %s"), os_strings[meta.os_guess]);
			s = g_string_append_c (s, '\n');

			/* avoid too many localtime() calls by doing it here */
			if (use_localtime)
			{
				t = time (NULL);
				tm = localtime (&t);
			}
			else
				tm = NULL;
			tz_name    = get_timezone_name (tm);
			tz_numeric = get_timezone_numeric (tm);
			g_string_append_printf (s, _("Time zone: %s [%s]"), tz_name, tz_numeric);
			s = g_string_append_c (s, '\n');
			s = g_string_append_c (s, '\n');

			if (meta.keep_deleted_entry)
				/* TRANSLATOR COMMENT: "Gone" means file is permanently deleted */
				g_string_append_printf (s, _("Index%sDeleted Time%sGone?%sSize%sPath"),
						delim, delim, delim, delim);
			else
				g_string_append_printf (s, _("Index%sDeleted Time%sSize%sPath"),
						delim, delim, delim);
			s = g_string_append_c (s, '\n');

			outstr = g_string_free (s, FALSE);
			_local_printf ("%s", outstr);
			g_free (outstr);
		}
		break;

		case OUTPUT_XML:
			/* No proper way to report wrong version info yet */
			_local_printf (
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<recyclebin format=\"%s\" version=\"%" G_GINT64_FORMAT "\">\n"
				"  <filename><![CDATA[%s]]></filename>\n",
				( meta.type == RECYCLE_BIN_TYPE_FILE ) ? "file" : "dir",
				MAX (meta.version, 0), rbin_path);
			break;

		default:
			g_assert_not_reached();
	}
	g_free (rbin_path);

	g_debug ("Leaving %s()", __func__);
}


void
print_record_cb (rbin_struct *record)
{
	char             *out_fname, *index, *size = NULL;
	char             *outstr = NULL, *deltime = NULL;
	GString          *t;
	struct tm         del_tm;

	g_return_if_fail (record != NULL);

	index = (record->meta->type == RECYCLE_BIN_TYPE_FILE) ?
		g_strdup_printf ("%u", record->index_n) :
		g_strdup (record->index_s);

	/*
	 * Used to check TZ environment variable here, but no more.
	 * Problems with that approach is now documented elsewhere.
	 */

	/*
	 * g_warning() further down below is an inline func that makes use of
	 * localtime(), and if localtime/gmtime is used here (it used to be),
	 * the value would be overwritten upon the g_warning call, into current
	 * machine time. Nasty.
	 *
	 * But why is the behavior occuring on MinGW-w64, but not even on MSYS2
	 * itself or any other OS; and why only manifesting now but not earlier?
	 * -- 2019-03-19
	 */
	if (use_localtime)
		localtime_r (&(record->deltime), &del_tm);
	else
		gmtime_r    (&(record->deltime), &del_tm);

	if ( record->legacy_path != NULL )
		out_fname = g_strdup (record->legacy_path);
	else
	{
		out_fname = record->uni_path ?
			g_strdup (record->uni_path) :
			g_strdup (_("(File name not representable in UTF-8 encoding)"));
	}

	switch (output_format)
	{
		case OUTPUT_CSV:

			if ((t = get_datetime_str (&del_tm)) != NULL)
				deltime = g_string_free (t, FALSE);
			else
			{
				g_warning (_("Error formatting file deletion time for record index %s."),
						index);
				deltime = g_strdup ("???");
			}

			if ( record->filesize == G_MAXUINT64 ) /* faulty */
				size = g_strdup ("???");
			else
				size = g_strdup_printf ("%" G_GUINT64_FORMAT, record->filesize);

			if (record->meta->keep_deleted_entry)
			{
				const char *purged = record->emptied ? _("Yes") : _("No");
				outstr = g_strjoin (delim, index, deltime, purged, size, out_fname, NULL);
			}
			else
				outstr = g_strjoin (delim, index, deltime, size, out_fname, NULL);

			_local_printf ("%s\n", outstr);

			break;

		case OUTPUT_XML:
		{
			GString *s = g_string_new (NULL);

			if ((t = get_iso8601_datetime_str (&del_tm)) != NULL)
				deltime = g_string_free (t, FALSE);
			else
			{
				g_warning (_("Error formatting file deletion time for record index %s."),
						index);
				deltime = g_strdup ("???");
			}

			g_string_printf (s, "  <record index=\"%s\" time=\"%s\"", index, deltime);

			if (record->meta->keep_deleted_entry)
				g_string_append_printf (s, " emptied=\"%c\"", record->emptied ? 'Y' : 'N');

			if ( record->filesize == G_MAXUINT64 ) /* faulty */
				size = g_strdup_printf (" size=\"-1\"");
			else
				size = g_strdup_printf (" size=\"%" G_GUINT64_FORMAT "\"", record->filesize);
			s = g_string_append (s, (const gchar*) size);

			g_string_append_printf (s,
				">\n"
				"    <path><![CDATA[%s]]></path>\n"
				"  </record>\n", out_fname);

			outstr = g_string_free (s, FALSE);
			_local_printf ("%s", outstr);
		}
			break;

		default:
			g_assert_not_reached();
	}
	g_free (outstr);
	g_free (out_fname);
	g_free (deltime);
	g_free (size);
	g_free (index);
}


void
print_footer (void)
{
	switch (output_format)
	{
		case OUTPUT_CSV:
			/* do nothing */
			break;

		case OUTPUT_XML:
			_local_printf ("%s", "</recyclebin>\n");
			break;

		default:
			g_assert_not_reached();
	}
}

void
close_output_handle (void)
{
	if (out_fh != NULL)
		fclose (out_fh);

#ifdef G_OS_WIN32
	close_wincon_handle();
#endif
}

r2status
move_temp_file (void)
{
	int e;

	if ( !tmppath || !output_loc )
		return EXIT_SUCCESS;

	if ( 0 == g_rename (tmppath, output_loc) )
		return EXIT_SUCCESS;

	e = errno;

	/* TRANSLATOR COMMENT: argument is system error message */
	g_printerr (_("Error moving output data to desinated file: %s"),
		g_strerror(e));
	g_printerr ("\n");

	/* TRANSLATOR COMMENT: argument is temp file location */
	g_printerr (_("Output content is left in '%s'."), tmppath);
	g_printerr ("\n");

	return R2_ERR_WRITE_FILE;
}

void
print_version_and_exit (void)
{
	fprintf (stdout, "%s %s\n", PACKAGE, VERSION);
	/* TRANSLATOR COMMENT: %s is software name */
	fprintf (stdout, _("%s is distributed under the "
		"BSD 3-Clause License.\n"), PACKAGE);
	/* TRANSLATOR COMMENT: 1st argument is software name, 2nd is official URL */
	fprintf (stdout, _("Information about %s can be found on\n\n\t%s\n"),
		PACKAGE, PACKAGE_URL);

	exit (EXIT_SUCCESS);
}


void
free_record_cb (rbin_struct *record)
{
	if ( record->meta->type == RECYCLE_BIN_TYPE_DIR )
		g_free (record->index_s);
	g_free (record->uni_path);
	g_free (record->legacy_path);
	g_free (record);
}


void
free_vars (void)
{
	g_strfreev (fileargs);
	g_free (output_loc);
	g_free (legacy_encoding);
	g_free (delim);
	g_free (tmppath);
}