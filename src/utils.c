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

#include "utils.h"
#include <glib/gi18n.h>

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
	extern gboolean always_utf8;

	va_start (args, format);
	utf_str = g_strdup_vprintf (format, args);
	va_end (args);

	g_return_if_fail (g_utf8_validate (utf_str, -1, NULL));

	if (always_utf8)
		fputs (utf_str, file);
	else
	{
		/* FIXME: shall catch error */
		char *locale_str =
			g_locale_from_utf8 (utf_str, -1, NULL, NULL, NULL);
		fputs (locale_str, file);
		g_free (locale_str);
	}
	g_free (utf_str);
}

void
print_header (FILE     *outfile,
              char     *infilename,
              int64_t   version,
              gboolean  is_info2)
{
	char           *utf8_filename, *ver_string;
	extern int      output_format;
	extern char    *delim;

	g_return_if_fail (infilename != NULL);
	g_return_if_fail (outfile != NULL);

	g_debug ("Entering %s()", __func__);

	if (g_path_is_absolute (infilename))
		utf8_filename = g_filename_display_basename (infilename);
	else
		utf8_filename = g_filename_display_name (infilename);

	switch (output_format)
	{
	  case OUTPUT_CSV:
		maybe_convert_fprintf (outfile, _("Recycle bin path: '%s'"),
		                       utf8_filename);
		fputs ("\n", outfile);
		switch (version)
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
			ver_string = g_strdup_printf ("%" G_GUINT64_FORMAT, version);
		}
		maybe_convert_fprintf (outfile, _("Version: %s"), ver_string);
		g_free (ver_string);
		fputs ("\n\n", outfile);

		if (is_info2)
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
		         (is_info2 ? "file" : "dir"), MAX (version, 0));
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
	char           *utf8_filename;
	char            ascii_deltime[21];
	struct tm      *tm;
	GError         *error = NULL;
	gboolean        is_info2;
	char           *index;

	extern char    *legacy_encoding;
	extern gboolean has_unicode_filename;
	extern gboolean use_localtime;
	extern int      output_format;
	extern char    *delim;
	extern gboolean always_utf8;

	g_return_if_fail (record != NULL);
	g_return_if_fail (outfile != NULL);

	is_info2 = (record->type == RECYCLE_BIN_TYPE_FILE);

	index = is_info2 ? g_strdup_printf ("%u", record->index_n) :
	                   g_strdup (record->index_s);

	tm = use_localtime ? localtime (&record->deltime) :
	                     gmtime    (&record->deltime);
	if (strftime (ascii_deltime, 20, "%Y-%m-%d %H:%M:%S", tm) == 0)
	{
		g_warning (_("Error formatting file deletion time for record index %s."),
		           index);
		strncpy ((char *) ascii_deltime, "???", 4);
	}

	if (has_unicode_filename && !legacy_encoding)	/* this part is info2 only */
		utf8_filename = g_strdup (record->utf8_filename);
	else
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

		fprintf (outfile, "%s%s%s%s", index, delim, ascii_deltime, delim);
		if (is_info2)
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
				g_warning (_("Error converting path name to "
				             "display for record %s: %s"),
				           index, error->message);
				g_clear_error (&error);
				shown =
					g_locale_from_utf8 (_("(File name not representable "
					                      "in current language)"),
					                    -1, NULL, NULL, NULL);
			}
			fprintf (outfile, "%s\n", shown);
			g_free (shown);
		}
		break;

	  case OUTPUT_XML:
		fprintf (outfile, "  <record index=\"%s\" time=\"%s\" ", index,
		         ascii_deltime);
		if (is_info2)
			fprintf (outfile, "emptied=\"%c\" ", record->emptied ? 'Y' : 'N');
		fprintf (outfile,
		         "size=\"%" G_GUINT64_FORMAT "\">\n    <path>%s</path>\n"
		         "  </record>\n",
		         record->filesize, utf8_filename);
		break;

	  default:
		g_warn_if_reached ();
	}
	g_free (utf8_filename);
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
