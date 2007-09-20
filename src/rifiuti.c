/*
 * Copyright (C) 2003, by Keith J. Jones.
 * Copyright (C) 2007, Abel Cheung.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "rifiuti.h"


static char      *delim                = "\t";
static char     **fileargs             = NULL;
static char      *outfilename          = NULL;
static char      *from_encoding        = NULL;
static gboolean   no_heading           = FALSE;
static gboolean   show_legacy_filename = FALSE;
static gboolean   xml_output           = FALSE;
static gboolean   always_utf8          = FALSE;
static gboolean   has_unicode_filename = FALSE;

static GOptionEntry entries[] =
{
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &outfilename,
    N_("Write output to FILE"), N_("FILE") },
  { "delimiter", 't', 0, G_OPTION_ARG_STRING, &delim,
    N_("String to use as delimiter (TAB by default)"), N_("STRING") },
  { "no-heading", 'n', 0, G_OPTION_ARG_NONE, &no_heading,
    N_("Don't show header"), NULL },
  { "legacy-filename", 'l', 0, G_OPTION_ARG_NONE, &show_legacy_filename,
    N_("Show legacy filename instead of unicode"), NULL },
  { "always-utf8", '8', 0, G_OPTION_ARG_NONE, &always_utf8,
    N_("Always show file names in UTF-8 encoding"), NULL },
  { "xml", 'x', 0, G_OPTION_ARG_NONE, &xml_output,
    N_("Output in XML format (-t, -n, -l, -8 options will have no effect)"), NULL },
  { "from-encoding", 0, 0, G_OPTION_ARG_STRING, &from_encoding,
    N_("The assumed file name character set when no unicode file name is present in INFO2 record (mandatory if INFO2 file is created by Win98, useless otherwise)"), N_("ENC") },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
    N_("INFO2 File names"), NULL },
  { NULL }
};


void print_header (FILE     *outfile,
                   char     *infilename,
                   uint32_t  version,
                   int       output_format)
{
  char *shown_filename;
  GError *error = NULL;

  switch (output_format)
  {
    case OUTPUT_CSV:

      if (!no_heading)
      {
        fprintf (outfile, _("Recycle bin file: '%s'\n"), infilename);
        fprintf (outfile, _("Version: %u\n\n"), version);
        fprintf (outfile, _("Index%sDeleted Time%sGone?%sSize%sPath\n"), delim, delim, delim, delim);
      }

      break;

    case OUTPUT_XML:

      fputs ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<recyclebin>\n", outfile);
      shown_filename = g_filename_to_utf8 (infilename, -1, NULL, NULL, &error);

      if (error)
      {
        fprintf (stderr, _("Error converting INFO2 file name to UTF-8: %s\n"),
                 error->message);
        g_free (shown_filename);
        shown_filename = g_strdup (_("(Invalid UTF-8 file name)"));
      }

      fprintf (outfile, "  <filename>%s</filename>\n", shown_filename);
      fprintf (outfile, "  <version>%u</version>\n", version);
      g_free (shown_filename);

      break;

    default:
      /* something is wrong */
      fputs (_("Unrecognized output format\n"), stderr);
      break;
  }
}


void print_record (FILE        *outfile,
                   info_struct *record,
                   int          output_format)
{
  char *shown_filename;
  char ascdeltime[21];
  GError *error = NULL;

  if (strftime (ascdeltime, 20, "%Y-%m-%d %H:%M:%S", record->filetime) == 0)
    fprintf (stderr, _("Error formatting deleted file date/time for index %u."), record->index);

  switch (output_format)
  {
    case OUTPUT_CSV:

      if (has_unicode_filename && !show_legacy_filename)
      {
        shown_filename = g_locale_from_utf8 (record->utf8_filename, -1, NULL, NULL, &error);
        if (error)
        {
          g_fprintf (stderr,
                     _("Error converting file name from UTF-8 to locale charset for record %u: %s\n"),
                     record->index, error->message);
          g_error_free (error);
          g_free (shown_filename);
          shown_filename = g_strdup (record->legacy_filename);
        }
      }
      else
        shown_filename = g_strdup (record->legacy_filename);

      fprintf (outfile, "%d%s%s%s%s%s%d%s%s\n",
               record->index                        , delim,
               ascdeltime                           , delim,
               record->emptied ? _("Yes") : _("No") , delim,
               record->filesize                     , delim,
               shown_filename);

      g_free (shown_filename);

      break;

    case OUTPUT_XML:

      fputs ("  <record>\n", outfile);
      fprintf (outfile, "    <index>%u</index>\n", record->index);
      fprintf (outfile, "    <time>%s</time>\n", ascdeltime);
      fprintf (outfile, "    <emptied>%c</emptied>\n", record->emptied ? 'Y' : 'N');
      fprintf (outfile, "    <size>%u</size>\n", record->filesize);

      if (has_unicode_filename)
        fprintf (outfile, "    <path>%s</path>\n", record->utf8_filename);
      else
      {
        /*
         * guessing charset is not useful, since the system generating INFO2 and the system
         * analyzing INFO2 would be different, and quite likely using different charset as well
         */
        shown_filename = g_convert (record->legacy_filename, -1, "UTF-8", from_encoding,
                                    NULL, NULL, &error);
        if (error)
        {
          g_fprintf (stderr,
                     _("Failed to convert file name from %s encoding to UTF-8 for record %u: %s\n"),
                     from_encoding, record->index, error->message);
          g_error_free (error);
          g_free (shown_filename);
          shown_filename = g_strdup (_("(Invalid file name)"));
        }
        fprintf (outfile, "    <path>%s</path>\n", shown_filename);
        g_free (shown_filename);
      }

      fputs ("  </record>\n", outfile);

      break;

    default:
      /* something is wrong */
      fputs (_("Unrecognized output format\n"), stderr);
      break;
  }
}


void print_footer (FILE *outfile,
                   int   output_format)
{
  switch (output_format)
  {
    case OUTPUT_CSV:
      /* do nothing */
      break;

    case OUTPUT_XML:
      fputs ("</recyclebin>\n", outfile);
      break;

    default:
      /* something is wrong */
      fputs (_("Unrecognized output format\n"), stderr);
      break;
  }
}


time_t win_filetime_to_epoch (uint64_t win_filetime)
{
  uint64_t epoch;

  /* I suppose millisecond resolution is not needed? -- Abel */
  epoch = (win_filetime - 116444736000000000LL) / 10000000;

  /* Will it go wrong? Hope not. */
  return (time_t) (epoch & 0xFFFFFFFF);
}


int main (int argc, char **argv)
{
  uint32_t recordsize, info2_version, dummy;
  void *buf;
  gboolean retval;
  int readstatus;
  FILE *infile, *outfile;
  char *infilename = NULL;
  int output_format = OUTPUT_CSV;

  info_struct *record;
  uint64_t win_filetime;
  time_t file_epoch;

  unsigned char driveletters[28] =
  {
    'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U',
    'V', 'W', 'X', 'Y', 'Z', '\\', '?'
  };

  GError *error = NULL;
  GOptionContext *context;


  /* FIXME: remove hardcoded values */
  setlocale (LC_ALL, "");
  bindtextdomain ("rifiuti", "/usr/share/locale");
  bind_textdomain_codeset ("rifiuti", "UTF-8");
  textdomain ("rifiuti");

  context = g_option_context_new (_("FILE"));
  g_option_context_add_main_entries (context, entries, "rifiuti");
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_option_context_free (context);

  if (error != NULL)
  {
    fprintf (stderr, _("Error parsing argument: %s\n"), error->message);
    g_error_free (error);
    exit (RIFIUTI_ERR_ARG);
  }

  if ( fileargs && g_strv_length (fileargs) > 1 )
  {
    fputs (_("ERROR: must provide no more than one file argument.\n"), stderr);
    exit (RIFIUTI_ERR_ARG);
  }

  if (fileargs)
  {
    infilename = strdup (fileargs[0]);
    infile = fopen (infilename, "rb");

    if (!infile)
    {
      g_fprintf (stderr, "Error opening file '%s' for reading: %s\n", infilename, strerror (errno));
      g_free (infilename);
      exit (RIFIUTI_ERR_OPEN_FILE);
    }

    g_free (fileargs[0]);
    g_free (fileargs);

  }
  else
  {
    infile = stdin;
    infilename = g_strndup ("-", 2);
  }

  if (outfilename)
  {
    outfile = fopen (outfilename, "wb");
    if (!outfile)
    {
      g_fprintf (stderr, "Error opening file '%s' for writing: %s\n", outfilename, strerror (errno));
      exit (RIFIUTI_ERR_OPEN_FILE);
    }
  }
  else
    outfile = stdout;

  if (xml_output)
    output_format = OUTPUT_XML;

  /* check for valid info2 file header */
  if ( !fread (&info2_version, 4, 1, infile) )
  {
    fprintf (stderr, _("ERROR: '%s' is not a valid INFO2 file.\n"), infilename);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }
  info2_version = GUINT32_FROM_LE (info2_version);

  if ( (info2_version != 4) && (info2_version != 5) )
  {
    fprintf (stderr, _("ERROR: '%s' is not a valid INFO2 file.\n"), infilename);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }

  if ( (4 == info2_version) && (OUTPUT_XML == output_format) && (!from_encoding) )
  {
    fputs (_("ERROR: for Win98 INFO2 file, original file name encoding must be specified with --from-encoding option if output is in XML format.\n"), stderr);
    exit (RIFIUTI_ERR_ARG);
  }

  /*
   * Skip for now, though they probably mean number of files left in Recycle bin
   * and last index, or some related number.
   */
  fread (&dummy, 4, 1, infile);
  fread (&dummy, 4, 1, infile);

  if ( !fread (&recordsize, 4, 1, infile) )
  {
    fprintf (stderr, _("ERROR: '%s' is not a valid INFO2 file.\n"), infilename);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }
  recordsize = GUINT32_FROM_LE (recordsize);

  /*
   * limit record item size to a ballpark figure; prevent corrupted file
   * or specially crafted INFO2 file (EEEEEEK!) from causing rifiuti allocating
   * too much memory
   */
  if ( recordsize > 65536 )
  {
    fputs (_("Size of record of each deleted item is too large."), stderr);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }

  /* only version 5 contains UCS2 filename */
  if ( (5 == info2_version) && (0x320 == recordsize) )
  {
    has_unicode_filename = TRUE;
  }

  print_header (outfile, infilename, info2_version, output_format);

  buf = g_malloc (recordsize);
  g_assert (buf);

  record = g_malloc (sizeof (info_struct));
  g_assert (record);

  while (1) {

    readstatus = fread (buf, recordsize, 1, infile);
    if (readstatus != 1)
    {
      if ( !feof (infile) )
        fprintf (stderr, _("ERROR: Failed to read next record: %s\n"), strerror (errno));

      /* FIXME: Also warn if last read is not exactly 4 bytes? */
      break;
    }

    /* Any legacy character set can contain embedded null byte? */
    record->legacy_filename = g_strndup ((char *) (buf + LEGACY_FILENAME_OFFSET),
                                         RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);

    memcpy (&record->index, buf + RECORD_INDEX_OFFSET, 4);
    record->index = GUINT32_FROM_LE (record->index);

    memcpy (&record->drive, buf + DRIVE_LETTER_OFFSET, 4);
    record->drive = GUINT32_FROM_LE (record->drive);

    /* first byte will be removed from filename if file is not in recycle bin */
    record->emptied = FALSE;
    if (!record->legacy_filename || !*record->legacy_filename)
    {
      record->emptied = TRUE;
      g_free (record->legacy_filename);

      /* 0-25 => A-Z, 26 => '\', 27 or above is erraneous(?) */
      if (record->drive > sizeof (driveletters) - 2)
      {
        record->drive = sizeof (driveletters) - 1;
        g_fprintf (stderr,
                   _("WARNING: Drive letter (0x%X) exceeded maximum (0x1A) for record %u.\n"),
                   record->drive, record->index);
      }

      record->legacy_filename = (char *) g_malloc (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);
      g_assert (record->legacy_filename);
      g_snprintf (record->legacy_filename, RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET,
                  "%c%s", driveletters[record->drive],
                  (char *) (buf + LEGACY_FILENAME_OFFSET + 1));
    }

    memcpy (&win_filetime, buf + FILETIME_OFFSET, 8);

    file_epoch = win_filetime_to_epoch (win_filetime);
    record->filetime = localtime (&file_epoch);

    memcpy (&record->filesize, buf + FILESIZE_OFFSET, 4);
    record->filesize = GUINT32_FROM_LE (record->filesize);

    if (has_unicode_filename)
    {
      record->utf8_filename = g_utf16_to_utf8 ((gunichar2 *) (buf + UNICODE_FILENAME_OFFSET),
                                               (recordsize - UNICODE_FILENAME_OFFSET) / 2,
                                               NULL, NULL, &error);
      /* not checking error, since Windows <= 2000 may insert junk after UCS2 file name */
      if (!record->utf8_filename)
      {
        g_fprintf (stderr,
                 _("Error converting UCS2 filename to UTF-8 for record %d: %s"),
                 record->index, error->message);
        g_error_free (error);
        record->utf8_filename = g_strdup (_("(Broken UCS2 file name)"));
      }
    }
    else
      record->utf8_filename = NULL;

    print_record (outfile, record, output_format);

    if (has_unicode_filename)
      g_free (record->utf8_filename);

    g_free (record->legacy_filename);

  }

  print_footer (outfile, output_format);

  fclose (infile);
  fclose (outfile);

  g_free (record);
  g_free (buf);
  g_free (infilename);

  exit (0);
}

/* vi: shiftwidth=2 expandtab tabstop=2
 */
