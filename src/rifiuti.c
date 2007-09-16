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

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "rifiuti.h"


static char *delim = "\t";
static char **fileargs = NULL;
static char *outfilename = NULL;
static gboolean no_heading = FALSE;
static gboolean show_legacy_filename = FALSE;

static GOptionEntry entries[] = {
  { "delimiter", 't', 0, G_OPTION_ARG_STRING, &delim,
    N_("String to use as delimiter (default is a TAB)"), N_("STRING") },
  { "no-heading", 0, 0, G_OPTION_ARG_NONE, &no_heading,
    N_("Don't show header"), NULL },
  { "legacy-filename", 0, 0, G_OPTION_ARG_NONE, &show_legacy_filename,
    N_("Show legacy filename instead of unicode"), NULL },
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &outfilename,
    N_("Write output to FILE"), N_("FILE") },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
    N_("INFO2 File names"), NULL },
  { NULL }
};


time_t win_filetime_to_epoch (uint64_t win_filetime) {

  uint64_t epoch;

  /* I suppose millisecond resolution is not needed? -- Abel */
  epoch = (win_filetime - 116444736000000000LL) / 10000000;

  /* Will it go wrong? Hope not. */
  return (time_t) (epoch & 0xFFFFFFFF);
}


int main (int argc, char **argv) {

  char ascdeltime[21];
  uint32_t recordsize, info2_version, dummy;
  void *buf;
  gboolean emptied = FALSE;
  gboolean retval;
  int readstatus;
  FILE *infile, *outfile;
  char *infilename = NULL;

  uint32_t index, drivenum, filesize;
  uint64_t win_filetime;
  time_t file_epoch;
  struct tm *delete_time;
  char *utf8_filename, *legacy_filename, *shown_filename;

  gboolean has_unicode_filename = FALSE;
  unsigned char driveletters[28] = {
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

  if (error != NULL) {
    fprintf (stderr, _("ERROR parsing argument: %s\n"), error->message);
    g_error_free (error);
    exit (RIFIUTI_ERR_ARG);
  }

  if ( fileargs && g_strv_length (fileargs) > 1 ) {
    fputs (_("ERROR: must provide no more than one file argument.\n"), stderr);
    exit (RIFIUTI_ERR_ARG);
  }

  if (fileargs) {
    infilename = strdup (fileargs[0]);
    infile = fopen (infilename, "rb");

    if (!infile) {
      g_fprintf (stderr, "ERROR opening file '%s' for reading: %s\n", infilename, strerror (errno));
      g_free (infilename);
      exit (RIFIUTI_ERR_OPEN_FILE);
    }

    g_free (fileargs[0]);
    g_free (fileargs);

  } else {
    infile = stdin;
    infilename = strndup ("-", 2);
  }

  if (outfilename) {
    outfile = fopen (outfilename, "wb");
    if (!outfile) {
      g_fprintf (stderr, "ERROR opening file '%s' for writing: %s\n", outfilename, strerror (errno));
      exit (RIFIUTI_ERR_OPEN_FILE);
    }
  } else {
    outfile = stdout;
  }

  /* check for valid info2 file header */
  if ( !fread (&info2_version, 4, 1, infile) ) {
    fprintf (stderr, _("ERROR: '%s' is not a valid INFO2 file.\n"), infilename);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }
  info2_version = GUINT32_FROM_LE (info2_version);

  if ( (info2_version != 4) && (info2_version != 5) ) {
    fprintf (stderr, _("ERROR: '%s' is not a valid INFO2 file.\n"), infilename);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }

  /*
   * Skip for now, though they probably mean number of files left in Recycle bin
   * and last index, or some related number.
   */
  fread (&dummy, 4, 1, infile);
  fread (&dummy, 4, 1, infile);

  if ( !fread (&recordsize, 4, 1, infile) ) {
    fprintf (stderr, _("ERROR: '%s' is not a valid INFO2 file.\n"), infilename);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }
  recordsize = GUINT32_FROM_LE (recordsize);

  /*
   * limit record item size to a ballpark figure; prevent corrupted file
   * or specially crafted INFO2 file (EEEEEEK!) from causing rifiuti allocating
   * too much memory
   */
  if ( recordsize > 65536 ) {
    fputs (_("Size of record of each deleted item is overly large."), stderr);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }

  /* only version 5 contains UCS2 filename */
  if ( (5 == info2_version) && (0x320 == recordsize) ) {
    has_unicode_filename = TRUE;
  }

  if (!no_heading) {
    fprintf (outfile, _("INFO2 File: '%s'\n"), infilename);
    fprintf (outfile, _("Version: %u\n"), info2_version);
    fprintf (outfile, _("Bytes per record: %u\n\n"), recordsize);
    fprintf (outfile, _("INDEX%sDELETED TIME%sGONE?%sSIZE%sPATH\n"), delim, delim, delim, delim);
  }

  buf = g_malloc (recordsize);
  g_assert (buf);

  while (1) {

    readstatus = fread (buf, recordsize, 1, infile);
    if (readstatus != 1) {
      if ( !feof (infile) ) {
        fprintf (stderr, _("ERROR: Failed to read next record: %s\n"), strerror (errno));
      }
      /* FIXME: Also warn if last read is not exactly 4 bytes? */
      break;
    }

    /* Any legacy character set can contain embedded null byte? */
    legacy_filename = strndup ((char *) (buf + LEGACY_FILENAME_OFFSET),
                               RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);

    memcpy (&index, buf + RECORD_INDEX_OFFSET, 4);
    index = GUINT32_FROM_LE (index);

    memcpy (&drivenum, buf + DRIVE_LETTER_OFFSET, 4);
    drivenum = GUINT32_FROM_LE (drivenum);

    /* first byte will be removed from filename if file is not in recycle bin */
    emptied = FALSE;
    if (!legacy_filename || !*legacy_filename) {
      emptied = TRUE;
      g_free (legacy_filename);

      /* 0-25 => A-Z, 26 => '\', 27 or above is erraneous(?) */
      if (drivenum > sizeof (driveletters) - 2) {
        drivenum = sizeof (driveletters) - 1;
        g_fprintf (stderr, _("WARNING: Drive letter (0x%X) exceeded maximum (0x1A) for index %u.\n"), drivenum, index);
      }

      legacy_filename = (char *) g_malloc (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);
      g_assert (legacy_filename);
      g_snprintf (legacy_filename, RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET,
                  "%c%s", driveletters[drivenum],
                  (char *) (buf + LEGACY_FILENAME_OFFSET + 1));
    }

    memcpy (&win_filetime, buf + FILETIME_OFFSET, 8);

    file_epoch = win_filetime_to_epoch (win_filetime);
    delete_time = localtime (&file_epoch);

    if (strftime (ascdeltime, 20, "%Y-%m-%d %H:%M:%S", delete_time) == 0) {
      fprintf (stderr, _("Error formatting deleted file date/time for index %u."), index);
    }

    memcpy (&filesize, buf + FILESIZE_OFFSET, 4);
    filesize = GUINT32_FROM_LE (filesize);

    if (has_unicode_filename && !show_legacy_filename) {

      utf8_filename = g_utf16_to_utf8 ((gunichar2 *) (buf + UNICODE_FILENAME_OFFSET),
                                       (recordsize - UNICODE_FILENAME_OFFSET) / 2,
                                       NULL, NULL, NULL);

      if (!utf8_filename) {
        fprintf (stderr, _("Error converting UCS2 filename to UTF-8, will show legacy filename for record %d"), index);
        shown_filename = legacy_filename;
      } else {
        shown_filename = utf8_filename;
      }

    } else {
      shown_filename = legacy_filename;
    }

    g_fprintf (outfile, "%d%s%s%s%s%s%d%s%s\n",
               index     , delim,
               ascdeltime, delim,
               emptied ? _("Yes") : _("No") , delim,
               filesize  , delim,
               shown_filename);

    if (has_unicode_filename) {
      g_free (utf8_filename);
    }
    g_free (legacy_filename);

  }

  fclose (infile);
  fclose (outfile);

  g_free (buf);
  g_free (infilename);

  exit (0);
}

/* vi: shiftwidth=2 expandtab tabstop=2
 */
