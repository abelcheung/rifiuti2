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
#include <sys/timeb.h>
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

#else

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
	char *i = (char *)str;

	if ((str == NULL) || (*str == '\0')) return NULL;

	result = g_string_new (NULL);
	do
	{
		if ( (*i) != '\\' )
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
		if (((*i) <= 0x7E) && ((*i) >= 0x20))
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
		fprintf (outfile, "%" G_GUINT64_FORMAT "%s",
		         (uint64_t) record->filesize, delim);

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
print_footer (
	FILE *outfile)
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

/* GUI message box */
#ifdef G_OS_WIN32

#include <windows.h>

static char *
convert_with_fallback (const char *string,
                       const char *fallback)
{
	GError *err = NULL;
	char   *output = g_locale_from_utf8 (string, -1, NULL, NULL, &err);
	if (err != NULL)
	{
		g_critical ("Failed to convert message to display: %s\n", err->message);
		g_clear_error (&err);
		return g_strdup (fallback);
	}

	return output;
}

void
gui_message (const char *message)
{
	char *title, *output;

	title = convert_with_fallback (_("This is a command line application"),
	                               "This is a command line application");
	output = convert_with_fallback (message,
	                                "Fail to display help message. Please "
	                                "invoke program with '--help' option.");

	MessageBox (NULL, output, title, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	g_free (title);
	g_free (output);
}

#endif

/* vim: set sw=4 ts=4 noexpandtab : */
