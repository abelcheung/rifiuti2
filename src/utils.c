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

#include <stdlib.h>
#if HAVE_SETLOCALE
#include <locale.h>
#endif
#include "utils.h"
#include <glib/gi18n.h>

#ifdef G_OS_WIN32
#  include <sys/timeb.h>
#  include "utils-win.h"
#endif

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

/* Return ISO8601 numeric timezone, like "+0400" */
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

/* Return ISO 8601 formatted time with timezone */
static GString *
get_iso8601_datetime_str (struct tm *tm)
{
	GString         *output;
	extern gboolean  use_localtime;

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
	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, my_debug_handler, NULL);

	setlocale (LC_ALL, "");

#ifdef G_OS_WIN32
	{
		char *loc = get_win32_locale();
		g_debug ("Use LC_MESSAGES = %s", loc);
		setlocale (LC_MESSAGES, loc);
		g_free (loc);
	}
#endif

	if (g_file_test (LOCALEDIR, G_FILE_TEST_IS_DIR))
		bindtextdomain (PACKAGE, LOCALEDIR);
	else
	{
		/* searching current dir is more useful on Windows */
		char *d = g_path_get_dirname (progpath);
		char *p = g_build_filename (d, LOCALEDIR_PORTABLE, NULL);
		if (g_file_test (p, G_FILE_TEST_IS_DIR))
		{
			g_debug ("Portable LOCALEDIR = %s", p);
			bindtextdomain (PACKAGE, p);
		}
		g_free (p);
		g_free (d);
	}
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);
}

void
rifiuti_setup_opt_ctx (GOptionContext **context,
                       GOptionEntry     opt_main[],
                       GOptionEntry     opt_add[])
{
	char *bug_report_str;
	GOptionGroup *textoptgroup;

	bug_report_str =
		g_strdup_printf (_("Report bugs to %s"), PACKAGE_BUGREPORT);
	g_option_context_set_description (*context, bug_report_str);
	g_free (bug_report_str);
	g_option_context_add_main_entries (*context, opt_main, PACKAGE);

	textoptgroup =
		g_option_group_new ("text", _("Plain text output options:"),
		                    N_("Show plain text output options"), NULL, NULL);
	g_option_group_set_translation_domain (textoptgroup, PACKAGE);
	g_option_group_add_entries (textoptgroup, opt_add);
	g_option_context_add_group (*context, textoptgroup);
}

int
rifiuti_parse_opt_ctx (GOptionContext **context,
                       int             *argc,
                       char          ***argv)
{
	GError   *err = NULL;
	gboolean  ret;

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

	/*
	 * The user case where this code won't provide benefit is VERY rare,
	 * so don't bother doing fallback because it was always the case before.
	 *
	 * However this parsing doesn't work nice with path translation in MSYS;
	 * directory separator in the middle of path would be translated to root
	 * of MSYS folder if earlier path component contains characters not in
	 * console codepage. Problem only observed in MSYS bash and not Windows
	 * console, so not a priority to fix.
	 */
#ifdef G_OS_WIN32
	{
		char **args = g_win32_get_command_line ();
		ret = g_option_context_parse_strv (*context, &args, &err);
		g_strfreev (args);
	}
#else
	ret = g_option_context_parse (*context, argc, argv, &err);
#endif
	g_option_context_free (*context);

	if ( !ret )
	{
		g_printerr (_("Error parsing options: %s\n"), err->message);
		g_error_free (err);
		return RIFIUTI_ERR_ARG;
	}
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

/*
 * Wrapper of g_utf16_to_utf8 for big endian system.
 * Always assume string is nul-terminated.
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

/*
 * single/double quotes and backslashes have already been
 * quoted / unquoted when parsing arguments. We need to
 * interpret \r, \n etc separately
 */
char *
filter_escapes (const char *str)
{
	GString *result, *debug_str;
	char *i = (char *) str;

	if ( !str || (!*str) ) return NULL;

	result = g_string_new (NULL);
	do
	{
		if ( *i != '\\' )
		{
			result = g_string_append_c (result, *i);
			continue;
		}
		switch ((char) (* (++i)))
		{
		  case 'r':
			result = g_string_append_c (result, '\r'); break;
		  case 'n':
			result = g_string_append_c (result, '\n'); break;
		  case 't':
			result = g_string_append_c (result, '\t'); break;
		  case 'v':
			result = g_string_append_c (result, '\v'); break;
		  case 'f':
			result = g_string_append_c (result, '\f'); break;
		  case 'e':
			result = g_string_append_c (result, '\x1B'); break;
		  default:
			result = g_string_append_c (result, '\\'); i--;
		}
	}
	while ((char) (* (++i)) != '\0');

	debug_str = g_string_new ("filtered delimiter = ");
	i = result->str;
	do
	{
		if ( *i >= 0x20 && *i <= 0x7E )  /* problem during linking with g_ascii_isprint */
			debug_str = g_string_append_c (debug_str, (*i));
		else
			g_string_append_printf (debug_str, "\\x%02X", (char) (*i));
	}
	while ((char) (* (++i)) != '\0');
	g_debug ("%s", debug_str->str);
	g_string_free (debug_str, TRUE);
	return g_string_free (result, FALSE);
}

void
my_debug_handler (const char     *log_domain,
                  GLogLevelFlags  log_level,
                  const char     *message,
                  gpointer        data)
{
	if (log_level != G_LOG_LEVEL_DEBUG) return;

	const char *val = g_getenv ("RIFIUTI_DEBUG");
	if (val != NULL)
		g_printerr ("DEBUG: %s\n", message);
}

/* Scan folder and add all "$Ixxxxxx.xxx" to filelist for parsing */
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
		g_printerr (_("Error opening directory '%s': %s\n"), path,
			error->message);
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


/* Search for desktop.ini in folder for hint of recycle bin */
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


/* Add potentially valid file(s) to list */
int
check_file_args (const char  *path,
                 GSList     **list,
                 gboolean     is_info2)
{
	g_debug ("Start basic file checking...");

	g_return_val_if_fail ( (path != NULL) && (list != NULL), RIFIUTI_ERR_INTERNAL );

	if ( !g_file_test (path, G_FILE_TEST_EXISTS) )
	{
		g_printerr (_("'%s' does not exist.\n"), path);
		return RIFIUTI_ERR_OPEN_FILE;
	}
	else if ( !is_info2 && g_file_test (path, G_FILE_TEST_IS_DIR) )
	{
		if ( ! _populate_index_file_list (list, path) )
			return RIFIUTI_ERR_OPEN_FILE;
		/*
		 * last ditch effort: search for desktop.ini. Just print empty content
		 * representing empty recycle bin if found.
		 */
		if ( !*list && !found_desktop_ini (path) )
		{
			g_printerr (_("No files with name pattern '%s' are found in directory.\n"),
					"$Ixxxxxx.*");
			return RIFIUTI_ERR_OPEN_FILE;
		}
	}
	else if ( g_file_test (path, G_FILE_TEST_IS_REGULAR) )
		*list = g_slist_prepend ( *list, g_strdup (path) );
	else
	{
		g_printerr (!is_info2 ? _("'%s' is not a normal file or directory.\n") :
		                        _("'%s' is not a normal file.\n"), path);
		return RIFIUTI_ERR_OPEN_FILE;
	}
	return EXIT_SUCCESS;
}


static gboolean
_local_printf (const char *format, ...)
{
	va_list        args;
	char          *str;
	extern FILE   *out_fh;

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
			g_critical (_("Error converting output from UTF-8 to UTF-16: %s\n"), err->message);
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


void
print_header (metarecord  meta)
{
	char             *utf8_filename, *ver_string;
	const char       *tz_name, *tz_numeric;
	extern int        output_format;
	extern char      *delim;
	time_t            t;
	struct tm        *tm;
	extern gboolean   use_localtime;
	extern FILE      *out_fh;

	g_return_if_fail (meta.filename != NULL);

	g_debug ("Entering %s()", __func__);

	utf8_filename = g_filename_display_name (meta.filename);

	switch (output_format)
	{
		case OUTPUT_CSV:
		{
			GString *s = g_string_new (NULL);
			char *outstr;

			g_string_printf (s, _("Recycle bin path: '%s'\n"), utf8_filename);

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
			g_string_append_printf (s, _("Version: %s\n"), ver_string);
			g_free (ver_string);

			if (meta.os_guess == OS_GUESS_UNKNOWN)
				s = g_string_append (s, _("OS detection failed\n"));
			else
				g_string_append_printf (s, _("OS Guess: %s\n"), os_strings[meta.os_guess]);

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
			g_string_append_printf (s, _("Time zone: %s [%s]\n\n"), tz_name, tz_numeric);

			if (meta.keep_deleted_entry)
				/* TRANSLATOR COMMENT: "Gone" means file is permanently deleted */
				g_string_append_printf (s, _("Index%sDeleted Time%sGone?%sSize%sPath\n"),
						delim, delim, delim, delim);
			else
				g_string_append_printf (s, _("Index%sDeleted Time%sSize%sPath\n"),
						delim, delim, delim);

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
				"  <filename>%s</filename>\n",
				( meta.type == RECYCLE_BIN_TYPE_FILE ) ? "file" : "dir",
				MAX (meta.version, 0), utf8_filename);
			break;

		default:
			g_assert_not_reached();
	}
	g_free (utf8_filename);

	g_debug ("Leaving %s()", __func__);
}


void
print_record_cb (rbin_struct *record)
{
	char             *utf8_filename, *index, *size = NULL;
	char             *outstr = NULL, *deltime = NULL;
	GString          *t;
	gboolean          is_info2;
	struct tm         del_tm;

	extern gboolean   use_localtime;
	extern char      *legacy_encoding;
	extern int        output_format;
	extern char      *delim;
	extern FILE      *out_fh;

	g_return_if_fail (record != NULL);

	is_info2 = (record->meta->type == RECYCLE_BIN_TYPE_FILE);

	index = is_info2 ? g_strdup_printf ("%u", record->index_n) :
	                   g_strdup (record->index_s);

	/*
	 * According to localtime() in MSDN: If the TZ environment variable is set,
	 * the C run-time library assumes rules appropriate to the United States for
	 * implementing the calculation of daylight-saving time (DST).
	 *
	 * This means DST start/stop time would be wrong for other parts of the world,
	 * and there is no way to indicate places whether DST is not +1 hour based on
	 * standard time. So providing facility for user adjustable TZ would be useless
	 * for non-US users. However, unsetting here still can't prevent user setting
	 * $TZ in command line. Throw my hands up and just document the problem.
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

	if (record->meta->has_unicode_path && !legacy_encoding)
	{
		utf8_filename = record->utf8_filename ?
			g_strdup (record->utf8_filename) :
			g_strdup (_("(File name not representable in UTF-8 encoding)"));
	}
	else	/* this part is info2 only */
	{
		GError *error = NULL;
		/*
		 * TODO Write a variant of g_convert_with_fallback that dumps hex chars
		 * instead of unicode escapes.
		 */
		utf8_filename =
			g_convert (record->legacy_filename, -1, "UTF-8", legacy_encoding,
			           NULL, NULL, &error);
		if (error)
		{
			g_warning (_("Error converting file name from %s encoding "
			             "to UTF-8 for index %s: %s"),
			           legacy_encoding, index, error->message);
			g_clear_error (&error);
			utf8_filename =
				g_strdup (_("(File name not representable in UTF-8 encoding)"));
		}
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
				outstr = g_strjoin (delim, index, deltime, purged, size, utf8_filename, NULL);
			}
			else
				outstr = g_strjoin (delim, index, deltime, size, utf8_filename, NULL);

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
				"    <path>%s</path>\n"
				"  </record>\n", utf8_filename);

			outstr = g_string_free (s, FALSE);
			_local_printf ("%s", outstr);
		}
			break;

		default:
			g_assert_not_reached();
	}
	g_free (outstr);
	g_free (utf8_filename);
	g_free (deltime);
	g_free (size);
	g_free (index);
}


void
print_footer (void)
{
	extern int output_format;

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
print_version ()
{
	fprintf (stdout, "%s %s\n", PACKAGE, VERSION);
	/* TRANSLATOR COMMENT: %s is software name */
	fprintf (stdout, _("%s is distributed under the "
				"BSD 3-Clause License.\n"), PACKAGE);
	/* TRANSLATOR COMMENT: 1st argument is software name, 2nd is official URL */
	fprintf (stdout, _("Information about %s can be found on\n\n\t%s\n"),
			PACKAGE, PACKAGE_URL);
}


void
free_record_cb (rbin_struct *record)
{
	if ( record->meta->type == RECYCLE_BIN_TYPE_DIR )
		g_free (record->index_s);
	g_free (record->utf8_filename);
	g_free (record->legacy_filename);
	g_free (record);
}
