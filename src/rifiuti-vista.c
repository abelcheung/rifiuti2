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
#include <string.h>
#include <errno.h>
#include <locale.h>

#include "utils.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "rifiuti-vista.h"

static char      *delim          = NULL;
static char     **fileargs       = NULL;
static char      *outfilename    = NULL;
static int        output_format  = OUTPUT_CSV;
static gboolean   no_heading     = FALSE;
static gboolean   xml_output     = FALSE;
static gboolean   always_utf8    = FALSE;
static gboolean   use_localtime  = FALSE;

static GOptionEntry mainoptions[] =
{
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &outfilename,
    N_("Write output to FILE"), N_("FILE") },
  { "xml", 'x', 0, G_OPTION_ARG_NONE, &xml_output,
    N_("Output in XML format instead (plain text options disallowed in this case)"), NULL },
  { "localtime", 'z', 0, G_OPTION_ARG_NONE, &use_localtime,
    N_("Present deletion time in time zone of local system (default is UTC)"), NULL },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
    N_("File names"), NULL },
  { NULL }
};

static GOptionEntry textoptions[] =
{
  { "delimiter", 't', 0, G_OPTION_ARG_STRING, &delim,
    N_("String to use as delimiter (TAB by default)"), N_("STRING") },
  { "no-heading", 'n', 0, G_OPTION_ARG_NONE, &no_heading,
    N_("Don't show header info"), NULL },
  { "always-utf8", '8', 0, G_OPTION_ARG_NONE, &always_utf8,
    N_("Always show file names in UTF-8 encoding"), NULL },
  { NULL }
};

void print_header (FILE *outfile,
                   char *infilename)
{
  char     *utf8_filename, *shown_filename;
  GError   *error = NULL;

  if (g_path_is_absolute (infilename))
    utf8_filename = g_filename_display_basename (infilename);
  else
    utf8_filename = g_filename_display_name (infilename);

  switch (output_format)
  {
    case OUTPUT_CSV:
      if (!no_heading)
      {
        if (!always_utf8)
        {
          shown_filename = g_locale_from_utf8 (utf8_filename, -1, NULL, NULL, &error);
          if (error)
          {
            g_warning (_("Error converting path name to display: %s"), error->message);
            g_free (shown_filename);
            shown_filename = g_strdup (_("(File name not representable in current language)"));
          }
          fprintf (outfile, _("Recycle bin file/dir: '%s'"), shown_filename);
          g_free (shown_filename);
        }
        else
          fprintf (outfile, _("Recycle bin file/dir: '%s'"), utf8_filename);

        fputs ("\n", outfile);
        fprintf (outfile, _("Version: %u"), 0);  /* FIXME to be implemented in future */
        fputs ("\n\n", outfile);
        fprintf (outfile, _("Index%sDeleted Time%sSize%sPath"), delim, delim, delim);
        fputs ("\n", outfile);
      }
      break;

    case OUTPUT_XML:
      fputs ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", outfile);
      fprintf (outfile, "<recyclebin format=\"dir\" version=\"%u\">\n", 0); /* to be implemented */
      fprintf (outfile, "  <filename>%s</filename>\n", utf8_filename);
      break;

    default:
      g_free (utf8_filename);
      g_return_if_reached();
      break;
  }

  g_free (utf8_filename);
}


/* Check if index file has sufficient amount of data for reading */
gboolean validate_index_file (FILE   *inf,
                              off_t   size)
{
  uint64_t version;
  uint32_t namelength;

  g_return_val_if_fail ((size > 0x18), FALSE);

  rewind (inf);
  fread (&version, 8, 1, inf);

  switch (version)
  {
    case FORMAT_VISTA:
      return ((size == 0x21F) || (size == 0x220));

    case FORMAT_WIN10:
      g_return_val_if_fail ((size > 0x1C), FALSE);
      fseek (inf, 0x18, SEEK_SET);
      fread (&namelength, 4, 1, inf);
      return (size == (0x1C + namelength * 2));

    default:
      return FALSE;
  }
}


rbin_struct *get_record_data (FILE     *inf,
                              uint64_t  version,
                              gboolean  erraneous)
{
  uint64_t     win_filetime;
  rbin_struct *record;
  time_t       file_epoch;
  gunichar2   *ucs2_filename;
  GError      *error = NULL;
  uint32_t     namelength;

  record = g_malloc0 (sizeof (rbin_struct));
  record->version = version;

  /*
   * In rare cases, the size of index file is 543 bytes versus (normal) 544 bytes.
   * In such occasion file size only occupies 56 bit, not 64 bit as it ought to be.
   * Actually this 56-bit file size is very likely wrong after all. Probably some
   * bug inside Windows. This is observed during deletion of dd.exe from Forensic
   * Acquisition Utilities by George M. Garner Jr.
   */
  fread (&record->filesize, (erraneous?7:8), 1, inf);

  /* File deletion time */
  fread (&win_filetime, 8, 1, inf);
  file_epoch = win_filetime_to_epoch (win_filetime);
  if (use_localtime)
    record->filetime = localtime (&file_epoch);
  else
    record->filetime = gmtime (&file_epoch);

  switch (version)
  {
    case FORMAT_VISTA:
      namelength = (uint32_t)WIN_PATH_MAX;
      break;

    case FORMAT_WIN10:
      fread (&namelength, 4, 1, inf);
      break;
  }

  /* One extra char for safety, though path should have already been null terminated */
  ucs2_filename = g_malloc0 (2 * (namelength + 1));
  fread (ucs2_filename, (size_t)namelength, 2, inf);

  record->utf8_filename = g_utf16_to_utf8 (ucs2_filename, -1, NULL, NULL, &error);

  if (error) {
    g_warning (_("Error converting file name to UTF-8 encoding: %s"), error->message);
    g_error_free (error);
  }

  g_free (ucs2_filename);
  return record;
}


void print_record (char *index_file,
                   FILE *outfile)
{
  FILE        *inf;
  rbin_struct *record;
  char         asctime[21];
  char        *basename;
  uint64_t     version;
  GStatBuf     st;

  if ( NULL == (inf = g_fopen (index_file, "rb")) )
  {
    g_warning (_("Error opening file '%s' for reading: %s"), index_file, strerror (errno));
    return;
  }

  if ( 0 != g_stat (index_file, &st) )
  {
    g_warning (_("Error getting metadata of file '%s': %s"), index_file, strerror (errno));
    return;
  }

  if ( !validate_index_file (inf, st.st_size) )
  {
    g_warning (_("Index file '%s' has incorrect size"), index_file);
    fclose (inf);
    return;
  }

  rewind (inf);
  fread (&version, 8, 1, inf);

  switch (version)
  {
    case FORMAT_VISTA:
      /* see get_record_data() comment on 2nd parameter */
      record = get_record_data (inf, version, (st.st_size == 0x21F));
      break;

    case FORMAT_WIN10:
      record = get_record_data (inf, version, FALSE);
      break;

    default:
      g_warning ( "'%s' is not recognized as recycle bin index file.", index_file );
      return;
  }

  if ( 0 == strftime (asctime, 20, "%Y-%m-%d %H:%M:%S", record->filetime) ) {
    g_warning (_("Error formatting file deletion time for file '%s'."), index_file);
    strncpy ((char*)asctime, "???", 4);
  }

  basename = g_path_get_basename (index_file);

  switch (output_format)
  {
    case OUTPUT_CSV:
      if (always_utf8)
        fprintf (outfile, "%s%s%s%s%" PRIu64 "%s%s\n",
            basename, delim, asctime, delim,
            record->filesize, delim, record->utf8_filename);
      else
      {
        char *localname = g_locale_from_utf8 (record->utf8_filename, -1, NULL, NULL, NULL);
        if (!localname)
          localname = g_strdup(_("(File name not representable in current language)"));

        fprintf (outfile, "%s%s%s%s%" PRIu64 "%s%s\n",
            basename, delim, asctime, delim,
            record->filesize, delim, localname);
        g_free (localname);
      }
      break;

    case OUTPUT_XML:
      fprintf (outfile, "  <record index=\"%s\" time=\"%s\" size=\"%" PRIu64 "\">\n",
               basename, asctime, record->filesize);
      fprintf (outfile, "    <path>%s</path>\n", record->utf8_filename);
      fputs ("  </record>\n", outfile);
      break;

    default:
      g_warn_if_reached();
      break;
  }

  fclose (inf);

  g_free (basename);
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
      g_return_if_reached();
      break;
  }
}


int main (int argc, char **argv)
{
  FILE *outfile;
  GPtrArray *filelist;
  char *fname;

  GError *error = NULL;
  GOptionContext *context;
  GOptionGroup *textoptgroup;


  setlocale (LC_ALL, "");
  /* searching current dir might be more useful on e.g. Windows */
  if (g_file_test (LOCALEDIR, G_FILE_TEST_IS_DIR))
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  else
    bindtextdomain (GETTEXT_PACKAGE, ".");
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_option_context_new (_("DIR_OR_FILE"));
  g_option_context_set_summary (context,
      _("Parse index files in \\$Recycle.bin style folder and dump recycle bin data. "
        "Can also dump a single index file."));
  g_option_context_add_main_entries (context, mainoptions, "rifiuti");

  textoptgroup = g_option_group_new ("text", _("Plain text output options:"),
                                     _("Show plain text output options"), NULL, NULL);
  g_option_group_set_translation_domain (textoptgroup, GETTEXT_PACKAGE);
  g_option_group_add_entries (textoptgroup, textoptions);
  g_option_context_add_group (context, textoptgroup);

  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    g_warning (_("Error parsing options: %s"), error->message);
    g_error_free (error);
    g_option_context_free (context);
    exit (RIFIUTI_ERR_ARG);
  }

  g_option_context_free (context);

  if ( !fileargs || g_strv_length (fileargs) > 1 )
  {
    g_warning (_("Must specify exactly one directory containing $Recycle.bin index files, "
          "or one such index file, as argument."));
    g_warning (_("Run program with '-?' option for more info."));
    exit (RIFIUTI_ERR_ARG);
  }

  if (outfilename)
  {
    outfile = g_fopen (outfilename, "wb");
    if (NULL == outfile) {
      g_critical (_("Error opening file '%s' for writing: %s"), outfilename, strerror (errno));
      exit (RIFIUTI_ERR_OPEN_FILE);
    }
  }
  else
    outfile = stdout;

  if (xml_output)
  {
    output_format = OUTPUT_XML;

    if ( no_heading || always_utf8 || (NULL != delim) )
    {
      g_warning (_("Plain text format options can not be used in XML mode."));
      exit (RIFIUTI_ERR_ARG);
    }
  }

  if (NULL == delim)
    delim = g_strndup ("\t", 2);

  filelist = g_ptr_array_new ();
  
  if (!g_file_test (fileargs[0], G_FILE_TEST_EXISTS))
  {
    g_critical (_("'%s' des not exist."), fileargs[0]);
    exit (RIFIUTI_ERR_OPEN_FILE);
  }
  else if (g_file_test (fileargs[0], G_FILE_TEST_IS_DIR))
  {
    /* Scan folder and add all "$Ixxxxxx.xxx" to filelist for parsing */
    GDir         *dir;
    char         *direntry;
    GPatternSpec *pattern1, *pattern2;

    if (NULL == (dir = g_dir_open (fileargs[0], 0, &error)))
    {
      g_critical (_("Error opening directory '%s': %s"), fileargs[0], error->message);
      g_error_free (error);
      exit (RIFIUTI_ERR_OPEN_FILE);
    }

    pattern1 = g_pattern_spec_new ("$I??????.*");
    pattern2 = g_pattern_spec_new ("$I??????");

    while ( (direntry = (char *) g_dir_read_name (dir)) != NULL )
    {
      if ( !g_pattern_match_string (pattern1, direntry) &&
           !g_pattern_match_string (pattern2, direntry) )
        continue;
      fname = g_build_filename (fileargs[0], direntry, NULL);
      g_ptr_array_add (filelist, fname);
    }

    g_dir_close (dir);

    g_pattern_spec_free (pattern1);
    g_pattern_spec_free (pattern2);

    if (filelist->len == 0)
    {
      g_critical (_("No files with name pattern \"$Ixxxxxx.xxx\" exists in directory. "
            "Is it really a $Recycle.bin directory?"));
      g_ptr_array_free (filelist, FALSE);
      exit (RIFIUTI_ERR_OPEN_FILE);
    }
  }
  else if (g_file_test (fileargs[0], G_FILE_TEST_IS_REGULAR))
  {
    fname = g_strdup (fileargs[0]);
    g_ptr_array_add (filelist, fname);
  }
  else
  {
    g_critical (_("'%s' is not a regular file or directory."), fileargs[0]);
    exit (RIFIUTI_ERR_BROKEN_FILE);
  }

  print_header (outfile, fileargs[0]);

  g_ptr_array_foreach (filelist, (GFunc) print_record, outfile);

  print_footer (outfile);

  g_ptr_array_foreach (filelist, (GFunc) g_free, NULL);
  g_ptr_array_free (filelist, TRUE);

  if (outfile != stdout)
    fclose (outfile);

  g_strfreev (fileargs);

  if (outfilename)
    g_free (outfilename);

  if (delim)
    g_free (delim);

  exit (0);
}

/* vim: set sw=2 expandtab ts=2 : */
