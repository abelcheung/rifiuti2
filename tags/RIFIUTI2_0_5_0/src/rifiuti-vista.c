/*
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include "utils.h"
#include "rifiuti-vista.h"

static char      *delim          = NULL;
static char     **fileargs       = NULL;
static char      *outfilename    = NULL;
static int        output_format  = OUTPUT_CSV;
static gboolean   no_heading     = FALSE;
static gboolean   xml_output     = FALSE;
static gboolean   always_utf8    = FALSE;

static GOptionEntry entries[] =
{
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &outfilename,
    N_("Write output to FILE"), N_("FILE") },
  { "xml", 'x', 0, G_OPTION_ARG_NONE, &xml_output,
    N_("Output in XML format"), NULL },
  { "always-utf8", '8', 0, G_OPTION_ARG_NONE, &always_utf8,
    N_("Always output file name in UTF-8 encoding"), NULL },
  { "no-heading", 'n', 0, G_OPTION_ARG_NONE, &no_heading,
    N_("Don't show header"), NULL },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
    N_("File names"), NULL },
  { NULL }
};


void print_header (FILE *outfile)
{
  switch (output_format)
  {
    case OUTPUT_CSV:
      if (!no_heading)
        fputs ("INDEX_FILE\tDELETION_TIME\tSIZE\tFILE_PATH\n", outfile);
      break;

    case OUTPUT_XML:
      fputs ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<recyclebin>\n", outfile);
      break;

    default:
      /* something is wrong */
      fputs (_("Unrecognized output format\n"), stderr);
      break;
  }
}


rbin_struct *get_record_data (FILE     *inf,
                              gboolean  erraneous)
{
  uint64_t version, win_filetime;
  rbin_struct *record;
  time_t file_epoch;
  gunichar2 ucs2_filename[0x104];
  GError *error = NULL;

  record = g_malloc0 (sizeof (rbin_struct));

  fread (&version, 8, 1, inf); /* useless for now */

  /*
   * In rare cases, the size of index file is 543 bytes versus (normal) 544 bytes.
   * In such occasion file size only occupies 56 bit, not 64 bit as it ought to be.
   * Actually this 56-bit file size is very likely misleading.
   */
  if (erraneous)
    fread (&record->filesize, 7, 1, inf);
  else
    fread (&record->filesize, 8, 1, inf);

  fread (&win_filetime, 8, 1, inf);

  file_epoch = win_filetime_to_epoch (win_filetime);
  record->filetime = localtime (&file_epoch);

  fread (&ucs2_filename, 2, 0x104, inf);

  record->utf8_filename = g_utf16_to_utf8 ((gunichar2 *)ucs2_filename, 0x104, NULL, NULL, &error);

  if (error) {
    fprintf (stderr, _("Error converting file name to UTF-8 encoding: %s\n"), error->message);
    g_error_free (error);
  }

  return record;
}


void print_record (char *index_file,
                   FILE *outfile)
{
  FILE *inf;
  rbin_struct *record;
  char asctime[21];
  struct stat filestat;

  if ( NULL == (inf = g_fopen (index_file, "rb")) )
  {
    g_fprintf (stderr, _("Error opening '%s' for reading: %s\n"), index_file, strerror (errno));
    return;
  }

  if ( 0 != g_stat (index_file, &filestat) )
  {
    g_fprintf (stderr, _("Error in stat() of file '%s': %s\n"), index_file, strerror (errno));
    return;
  }

  if ( filestat.st_size == 0x220 )
    record = get_record_data (inf, FALSE);
  else if ( filestat.st_size == 0x21F ) /* rare, and looks like a bug */
    record = get_record_data (inf, TRUE);
  else
  {
    fprintf (stderr, _("'%s' is probably not a recycle bin index file.\n"), index_file);
    fclose (inf);
    return;
  }

  if ( 0 == strftime (asctime, 20, "%Y-%m-%d %H:%M:%S", record->filetime) )
    fprintf (stderr, _("Error formatting deletion date/time for file '%s'."), index_file);

  switch (output_format)
  {
    case OUTPUT_CSV:
      if (always_utf8)
        fprintf (outfile, "%s\t%s\t%llu\t%s\n", index_file, asctime,
                 record->filesize, record->utf8_filename);
      else
      {
        char *localname = g_locale_from_utf8 (record->utf8_filename, -1, NULL, NULL, NULL);
        if (localname)
          fprintf (outfile, "%s\t%s\t%llu\t%s\n", index_file, asctime,
                   record->filesize, localname);
        else
          fprintf (outfile, "%s\t%s\t%llu\t%s\n", index_file, asctime, record->filesize,
                   _("(File name not representable in current locale charset)"));
        g_free (localname);
      }
      break;

    case OUTPUT_XML:
      fputs ("  <record>\n", outfile);
      fprintf (outfile, "    <indexfile>%s</indexfile>\n", index_file);
      fprintf (outfile, "    <time>%s</time>\n", asctime);
      fprintf (outfile, "    <size>%llu</size>\n", record->filesize);
      fprintf (outfile, "    <path>%s</path>\n", record->utf8_filename);
      fputs ("  </record>\n", outfile);
      break;

    default:
      /* something is wrong */
      fputs (_("Unrecognized output format\n"), stderr);
      break;
  }

  fclose (inf);

  g_free (record->utf8_filename);
  g_free (record);
}


void print_footer (FILE *outfile)
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


int main (int argc, char **argv)
{
  FILE *outfile;
  GPtrArray *filelist;
  char *fname;
  GPatternSpec *pattern1, *pattern2;
  int i;

  GError *error = NULL;
  GOptionContext *context;


  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_option_context_new (_("[FILE_OR_DIR ...]"));
  g_option_context_add_main_entries (context, entries, "rifiuti");

  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    fprintf (stderr, _("Error parsing argument: %s\n"), error->message);
    g_error_free (error);
    g_option_context_free (context);
    exit (RIFIUTI_ERR_ARG);
  }

  g_option_context_free (context);


  if (!outfilename)
    outfile = stdout;
  else
  {
    outfile = g_fopen (outfilename, "wb");
    if (!outfile)
    {
      g_fprintf (stderr, _("Error opening file '%s' for writing: %s\n"), outfilename, strerror (errno));
      exit (RIFIUTI_ERR_OPEN_FILE);
    }
  }


  if (xml_output)
    output_format = OUTPUT_XML;

  if ( (OUTPUT_XML == output_format) && always_utf8 )
  {
    fprintf (stderr, _("--always-utf8 option can not be used in XML output mode.\n"));
    exit (RIFIUTI_ERR_ARG);
  }

  pattern1 = g_pattern_spec_new ("$I??????.*");
  pattern2 = g_pattern_spec_new ("$I??????");
  filelist = g_ptr_array_new ();
  
  if (!fileargs)
  {
    fname = g_strndup ("-", 2);
    g_ptr_array_add (filelist, fname);
  }
  else
  {
    for (i = 0; i < g_strv_length (fileargs); i++)
    {
      if (g_file_test (fileargs[i], G_FILE_TEST_IS_DIR))
      {
        GDir *dir;
        char *direntry;

        if (NULL == (dir = g_dir_open (fileargs[i], 0, &error)))
        {
          g_fprintf (stderr, _("Error opening directory '%s': %s\n"), fileargs[i], error->message);
          g_error_free (error);
          continue;
        }

        while ( (direntry = (char *) g_dir_read_name (dir)) != NULL )
        {
          if ( !g_pattern_match_string (pattern1, direntry) &&
               !g_pattern_match_string (pattern2, direntry) )
            continue;
          fname = g_build_filename (fileargs[i], direntry, NULL);
          g_ptr_array_add (filelist, fname);
        }

        g_dir_close (dir);
      }
      else if (g_file_test (fileargs[i], G_FILE_TEST_IS_REGULAR))
      {
        fname = g_strdup (fileargs[i]);
        g_ptr_array_add (filelist, fname);
      }
    }
  }

  g_pattern_spec_free (pattern1);
  g_pattern_spec_free (pattern2);

  print_header (outfile);

  g_ptr_array_foreach (filelist, (GFunc) print_record, outfile);

  print_footer (outfile);

  g_ptr_array_foreach (filelist, (GFunc) g_free, NULL);

  g_ptr_array_free (filelist, TRUE);

  if (outfile != stdout)
    fclose (outfile);

  if (fileargs)
    g_strfreev (fileargs);

  if (outfilename)
    g_free (outfilename);

  if (delim)
    g_free (delim);

  exit (0);
}

/* vi: shiftwidth=2 expandtab tabstop=2
 */
