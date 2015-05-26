/*
 * Copyright (C) 2007, 2015 Abel Cheung.
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
#include "utils.h"
#include <glib/gi18n.h>

#ifdef G_OS_WIN32
#  include <sys/timeb.h>
#  include "utils-win.h"
#endif

static GString *
get_datetime_str (time_t  t,
                  int    *is_dst)
{
	GString         *output;
	size_t           len;
	struct tm       *tm;
	extern gboolean  use_localtime;

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
	tm = use_localtime ? localtime (&t) : gmtime (&t);
	if ( is_dst != NULL )
		*is_dst = tm->tm_isdst;
	output = g_string_sized_new (40);
	len = strftime (output->str, output->allocated_len, "%Y-%m-%d %H:%M:%S", tm);
	if ( !len )
	{
		g_string_free (output, TRUE);
		return NULL;
	}

	output->len = len;   /* is this unorthodox? */
	return output;
}

/*
 * Turns out strftime is not so cross-platform, Windows one supports far
 * less format strings than Unix counterpart.
 * However, GDateTime is not available until 2.26, so bite the bullet.
 *
 * Returns ISO 8601 formatted time.
 */
static GString *
get_iso8601_datetime_str (time_t t)
{
	GString         *output;
	extern gboolean  use_localtime;
	int              is_dst;
#ifdef G_OS_WIN32
	struct _timeb    tstruct;
	int              offset;
#else
	size_t           len;
	struct tm       *tm;
#endif

	if ( ( output = get_datetime_str (t, &is_dst) ) == NULL )
		return NULL;

	output->str[10] = 'T';
	if ( !use_localtime )
		return g_string_append_c (output, 'Z');

#ifdef G_OS_WIN32
	_ftime (&tstruct);
	/*
	 * 1. timezone value is in opposite sign of what people expect
	 * 2. it doesn't account for DST.
	 * 3. tm.tm_isdst is merely a flag and not indication on difference of
	 *    hours between DST and standard time. But there is no way to
	 *    override timezone in C library other than $TZ, and it always use
	 *    US rule, so again, just give up and use the value
	 */
	offset = MAX(is_dst, 0) * 60 - tstruct.timezone;
	g_string_append_printf (output, "%+.2i%.2i", offset / 60,
	                        abs(offset) % 60);
#else /* !def G_OS_WIN32 */
	tm = localtime (&t);
	len = strftime (output->str + output->len,
	                output->allocated_len - output->len, "%z", tm);
	if ( !len )
	{
		g_string_free (output, TRUE);
		return NULL;
	}
	output->len += len;
#endif

	return output;
}


void
rifiuti_init (char *progpath)
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
		bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	else
	{
		/* searching current dir is more useful on Windows */
		char *d = g_path_get_dirname (progpath);
		char *p = g_build_filename (d, "rifiuti-l10n", NULL);
		if (g_file_test (p, G_FILE_TEST_IS_DIR))
		{
			g_debug ("Alternative LOCALEDIR = %s", p);
			bindtextdomain (GETTEXT_PACKAGE, p);
		}
		g_free (p);
		g_free (d);
	}
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
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
	g_option_context_add_main_entries (*context, opt_main, GETTEXT_PACKAGE);

	textoptgroup =
		g_option_group_new ("text", _("Plain text output options:"),
		                    N_("Show plain text output options"), NULL, NULL);
	g_option_group_set_translation_domain (textoptgroup, GETTEXT_PACKAGE);
	g_option_group_add_entries (textoptgroup, opt_add);
	g_option_context_add_group (*context, textoptgroup);
}


void
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
#if GLIB_CHECK_VERSION(2, 40, 0) && defined (G_OS_WIN32)
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
		exit (RIFIUTI_ERR_ARG);
	}
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

void
maybe_convert_fprintf (FILE       *file,
                       const char *format, ...)
{
	va_list         args;
	char           *utf_str;
	const char     *charset;
	extern gboolean always_utf8;

	va_start (args, format);
	utf_str = g_strdup_vprintf (format, args);
	va_end (args);

	g_return_if_fail (g_utf8_validate (utf_str, -1, NULL));

	if (always_utf8 || g_get_charset (&charset))
		fputs (utf_str, file);
	else
	{
		char *locale_str =
			g_convert_with_fallback (utf_str, -1, charset, "UTF-8", NULL,
			                         NULL, NULL, NULL);
		fputs (locale_str, file);
		g_free (locale_str);
	}
	g_free (utf_str);
}


/* Scan folder and add all "$Ixxxxxx.xxx" to filelist for parsing */
static void
populate_index_file_list (GSList     **list,
                          const char  *path)
{
	GDir           *dir;
	char           *direntry, *fname;
	GPatternSpec   *pattern1, *pattern2;
	GError         *error = NULL;

	if (NULL == (dir = g_dir_open (path, 0, &error)))
	{
		g_printerr (_("Error opening directory '%s': %s\n"), path,
		            error->message);
		g_clear_error (&error);
		exit (RIFIUTI_ERR_OPEN_FILE);
	}

	pattern1 = g_pattern_spec_new ("$I??????.*");
	pattern2 = g_pattern_spec_new ("$I??????");

	while ((direntry = (char *) g_dir_read_name (dir)) != NULL)
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
void
check_file_args (const char  *path,
                 GSList     **list,
                 gboolean     is_info2)
{
	g_debug ("Start basic file checking...");

	g_return_if_fail ( (path != NULL) && (list != NULL) );

	if ( !g_file_test (path, G_FILE_TEST_EXISTS) )
	{
		g_printerr (_("'%s' does not exist.\n"), path);
		exit (RIFIUTI_ERR_OPEN_FILE);
	}
	else if ( !is_info2 && g_file_test (path, G_FILE_TEST_IS_DIR) )
	{
		populate_index_file_list (list, path);
		/*
		 * last ditch effort: search for desktop.ini. Just print empty content
		 * representing empty recycle bin if found.
		 */
		if ( *list || found_desktop_ini (path) ) return;

		g_printerr (_("No files with name pattern '%s' are found in directory. "
					"Probably not a $Recycle.bin directory.\n"), "$Ixxxxxx.*");
		exit (RIFIUTI_ERR_OPEN_FILE);
	}
	else if ( g_file_test (path, G_FILE_TEST_IS_REGULAR) )
		*list = g_slist_prepend ( *list, g_strdup (path) );
	else
	{
		g_printerr (!is_info2 ? _("'%s' is not a normal file or directory.\n") :
		                        _("'%s' is not a normal file.\n"), path);
		exit (RIFIUTI_ERR_OPEN_FILE);
	}
}


void
print_header (FILE       *outfile,
              metarecord  meta)
{
	char           *utf8_filename, *ver_string;
	extern int      output_format;
	extern char    *delim;

	g_return_if_fail (meta.filename != NULL);
	g_return_if_fail (outfile != NULL);

	g_debug ("Entering %s()", __func__);

	utf8_filename = g_filename_display_name (meta.filename);

	switch (output_format)
	{
	  case OUTPUT_CSV:
		maybe_convert_fprintf (outfile, _("Recycle bin path: '%s'"),
		                       utf8_filename);
		fputs ("\n", outfile);
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
		maybe_convert_fprintf (outfile, _("Version: %s"), ver_string);
		g_free (ver_string);
		fputs ("\n\n", outfile);

		if (meta.keep_deleted_entry)
			/* TRANSLATOR COMMENT: "Gone" means file is permanently deleted */
			maybe_convert_fprintf (outfile,
			                       _("Index%sDeleted Time%sGone?%sSize%sPath"),
			                       delim, delim, delim, delim);
		else
			maybe_convert_fprintf (outfile,
			                       _("Index%sDeleted Time%sSize%sPath"),
			                       delim, delim, delim);
		fputs ("\n", outfile);
		break;

	  case OUTPUT_XML:
		fputs ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", outfile);
		/* No proper way to report wrong version info yet */
		fprintf (outfile,
		         "<recyclebin format=\"%s\" version=\"%" G_GINT64_FORMAT "\">\n",
		         ( meta.type == RECYCLE_BIN_TYPE_FILE ) ? "file" : "dir",
		         MAX (meta.version, 0));
		fprintf (outfile, "  <filename>%s</filename>\n", utf8_filename);
		break;

	  default:
		g_warn_if_reached ();
	}
	g_free (utf8_filename);

	g_debug ("Leaving %s()", __func__);
}


void
print_record (rbin_struct *record,
              FILE        *outfile)
{
	char           *utf8_filename, *timestr = NULL;
	GString        *temp_timestr;
	GError         *error = NULL;
	gboolean        is_info2;
	char           *index;

	extern char    *legacy_encoding;
	extern gboolean has_unicode_filename;
	extern int      output_format;
	extern char    *delim;
	extern gboolean always_utf8;

	g_return_if_fail (record != NULL);
	g_return_if_fail (outfile != NULL);

	is_info2 = (record->meta->type == RECYCLE_BIN_TYPE_FILE);

	index = is_info2 ? g_strdup_printf ("%u", record->index_n) :
	                   g_strdup (record->index_s);

	if (has_unicode_filename && !legacy_encoding)
	{
		utf8_filename = record->utf8_filename ?
			g_strdup (record->utf8_filename) :
			g_strdup (_("(File name not representable in UTF-8 encoding)"));
	}
	else	/* this part is info2 only */
	{
		/* 
		 * On Windows, conversion from the file path's legacy charset to display codepage
		 * charset is most likely not supported unless the 2 legacy charsets happen to be
		 * equal. Try <legacy> -> UTF-8 -> <codepage> and see which step fails.
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

		if ( NULL == ( temp_timestr = get_datetime_str (record->deltime, NULL) ) )
		{
			g_warning (_("Error formatting file deletion time for record index %s."),
					   index);
			timestr = g_strdup ("???");
		}
		else
			timestr = g_string_free (temp_timestr, FALSE);

		fprintf (outfile, "%s%s%s%s", index, delim, timestr, delim);
		if (record->meta->keep_deleted_entry)
			maybe_convert_fprintf (outfile, "%s%s",
			                       record->emptied ? _("Yes") : _("No"),
			                       delim);
		if ( record->filesize == G_MAXUINT64 ) /* faulty */
			fprintf (outfile, "???%s", delim);
		else
			fprintf (outfile, "%" G_GUINT64_FORMAT "%s",
			         record->filesize, delim);

		if (always_utf8)
			fprintf (outfile, "%s\n", utf8_filename);
		else
		{
			char *shown =
				g_locale_from_utf8 (utf8_filename, -1, NULL, NULL, &error);
			if (error)
			{
				g_warning (_("Error converting path name to display for record %s: %s"),
				           index, error->message);
				g_clear_error (&error);
				shown = g_locale_from_utf8 (
						_("(File name not representable in current language)"),
						-1, NULL, NULL, NULL);
			}
			fprintf (outfile, "%s\n", shown);
			g_free (shown);
		}
		break;

	  case OUTPUT_XML:

		if ( NULL == ( temp_timestr = get_iso8601_datetime_str (record->deltime) ) )
		{
			g_warning (_("Error formatting file deletion time for record index %s."),
					   index);
			timestr = g_strdup ("???");
		}
		else
			timestr = g_string_free (temp_timestr, FALSE);

		fprintf (outfile, "  <record index=\"%s\" time=\"%s\" ", index,
		         timestr);
		if (record->meta->keep_deleted_entry)
			fprintf (outfile, "emptied=\"%c\" ", record->emptied ? 'Y' : 'N');
		fprintf (outfile,
		         "size=\"%" G_GUINT64_FORMAT "\">\n"
		         "    <path>%s</path>\n"
		         "  </record>\n",
		         record->filesize, utf8_filename);
		break;

	  default:
		g_warn_if_reached ();
	}
	g_free (utf8_filename);
	g_free (timestr);
	g_free (index);
}


void
print_version ()
{
	maybe_convert_fprintf (stdout, "%s %s\n", PACKAGE, VERSION);
	/* TRANSLATOR COMMENT: %s is software name */
	maybe_convert_fprintf (stdout,
	                       _("%s is distributed under the "
	                         "BSD 3-Clause License.\n"), PACKAGE);
	/* TRANSLATOR COMMENT: 1st argument is software name, 2nd is official URL */
	maybe_convert_fprintf (stdout, _("Information about %s can be found on\n\n\t%s\n"),
	                       PACKAGE, PACKAGE_URL);
	exit (EXIT_SUCCESS);
}


void
free_record (rbin_struct *record)
{
	if ( record->meta->type == RECYCLE_BIN_TYPE_DIR )
		g_free (record->index_s);
	g_free (record->utf8_filename);
	g_free (record->legacy_filename);
	g_free (record);
}


void
print_footer (FILE *outfile)
{
	extern int output_format;

	g_return_if_fail (outfile != NULL);

	g_debug ("Entering %s()", __func__);

	switch (output_format)
	{
	  case OUTPUT_CSV:
		/* do nothing */
		break;

	  case OUTPUT_XML:
		fputs ("</recyclebin>\n", outfile);
		break;

	  default:
		g_return_if_reached ();
		break;
	}
	g_debug ("Leaving %s()", __func__);
}


/* vim: set sw=4 ts=4 noexpandtab : */
