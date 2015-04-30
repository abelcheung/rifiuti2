/*
 * Copyright (C) 2003, by Keith J. Jones.
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

#include <errno.h>
#include <stdlib.h>

#include "utils.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "rifiuti.h"


       char      *delim                = NULL;
static char     **fileargs             = NULL;
static char      *outfilename          = NULL;
static char      *legacy_encoding      = NULL;
       int        output_format        = OUTPUT_CSV;
static gboolean   no_heading           = FALSE;
static gboolean   xml_output           = FALSE;
       gboolean   always_utf8          = FALSE;
static gboolean   has_unicode_filename = FALSE;
static gboolean   use_localtime        = FALSE;

static GOptionEntry mainoptions[] =
{
  { "output"         , 'o', 0, G_OPTION_ARG_FILENAME      , &outfilename,
    N_("Write output to FILE"), N_("FILE") },
  { "xml"            , 'x', 0, G_OPTION_ARG_NONE          , &xml_output,
    N_("Output in XML format instead (plain text options disallowed in this case)"), NULL },
  { "legacy-filename", 'l', 0, G_OPTION_ARG_STRING        , &legacy_encoding,
    N_("Show legacy (8.3) filename if available, and specify its CODEPAGE to use "
       "(option is mandatory if INFO2 file is created by Win98)"), N_("CODEPAGE") },
  { "localtime"      , 'z', 0, G_OPTION_ARG_NONE          , &use_localtime,
    N_("Present deletion time in time zone of local system (default is UTC)"), NULL },
  { G_OPTION_REMAINING, 0 , 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
    N_("INFO2 File names"), NULL },
  { NULL }
};

static GOptionEntry textoptions[] =
{
  { "delimiter"  , 't', 0, G_OPTION_ARG_STRING, &delim,
    N_("String to use as delimiter (TAB by default)"), N_("STRING") },
  { "no-heading" , 'n', 0, G_OPTION_ARG_NONE  , &no_heading,
    N_("Don't show header info"), NULL },
  { "always-utf8", '8', 0, G_OPTION_ARG_NONE  , &always_utf8,
    N_("Always show file names in UTF-8 encoding"), NULL },
  { NULL }
};

static void print_record (FILE        *outfile,
                          rbin_struct *record)
{
  char   *utf8_filename;
  GError *error = NULL;
  char    ascii_deltime[21];

  if (strftime (ascii_deltime, 20, "%Y-%m-%d %H:%M:%S", record->filetime) == 0)
  {
    g_warning (_("Error formatting file deletion time for record %u."), record->index);
    strncpy ((char*) ascii_deltime, "???", 4);
  }

  if (has_unicode_filename && !legacy_encoding)
    utf8_filename = g_strdup (record->utf8_filename);
  else
  {
    /* 
     * On Windows, conversion from the file path's legacy charset to display codepage
     * charset is most likely not supported unless the 2 legacy charsets happen to be
     * equal. Try <legacy> -> UTF-8 -> <codepage> and see which step fails.
     */
    utf8_filename = g_convert (record->legacy_filename, -1, "UTF-8", legacy_encoding,
                                NULL, NULL, &error);
    if (error)
    {
      g_warning (_("Error converting file name from %s encoding to UTF-8 for record %u: %s"),
                 legacy_encoding, record->index, error->message);
      g_error_free (error);
      utf8_filename = g_strdup (_("(File name not representable in UTF-8 encoding)"));
    }
  }

  switch (output_format)
  {
    case OUTPUT_CSV:

      fprintf (outfile, "%d%s%s%s", record->index, delim, ascii_deltime, delim);
      maybe_convert_fprintf (outfile, "%s", record->emptied ? _("Yes") : _("No"));
      fprintf (outfile, "%s%d%s", delim, record->filesize, delim);
      if (always_utf8)
        fprintf (outfile, "%s\n", utf8_filename);
      else
      {
        char *shown = g_locale_from_utf8 (utf8_filename, -1, NULL, NULL, &error);
        if (error)
        {
          g_warning (_("Error converting path name to display for record %u: %s"),
                     record->index, error->message);
          g_error_free (error);
          shown = g_locale_from_utf8 (_("(File name not representable in current language)"),
              -1, NULL, NULL, NULL);
        }
        fprintf (outfile, "%s\n", shown);
        g_free (shown);
      }
      break;

    case OUTPUT_XML:

      fprintf (outfile, "  <record index=\"%u\" time=\"%s\" emptied=\"%c\" size=\"%u\">\n"
                        "    <path>%s</path>\n"
                        "  </record>\n",
                        record->index, ascii_deltime,
                        record->emptied ? 'Y' : 'N',
                        record->filesize, utf8_filename);
      break;

    default:
      g_return_if_reached();
  }
  g_free (utf8_filename);
}

/* Check if index file has sufficient amount of data for reading */
/* 0 = success, all other return status = error */
static int validate_index_file (FILE     *inf,
                                off_t     size,
                                uint32_t *info2_version,
                                uint32_t *recordsize)
{
  size_t status;

  g_debug ("Start file validation...");

  if (size < RECORD_START_OFFSET) /* empty INFO2 file has 20 bytes */
  {
    g_debug ("file size = %d, expect at least %d\n", (int)size, RECORD_START_OFFSET);
    g_critical (_("This INFO2 file is truncated, or probably not an INFO2 file."));
    return RIFIUTI_ERR_BROKEN_FILE;
  }

  /* with file size check already done, fread fail probably mean serious problem */
  fseek (inf, 0, SEEK_SET);
  status = fread (info2_version, sizeof(*info2_version), 1, inf);
  if ( status < 1 )
  {
    /* TRANSLATOR COMMENT: the variable is function name */
    g_critical (_("%s(): fread() failed when reading info2_version"), __func__);
    return RIFIUTI_ERR_OPEN_FILE;
  }
  *info2_version = GUINT32_FROM_LE (*info2_version);

  fseek (inf, RECORD_SIZE_OFFSET, SEEK_SET);
  status = fread (recordsize, sizeof(*recordsize), 1, inf);
  if ( status < 1 )
  {
    /* TRANSLATOR COMMENT: the variable is function name */
    g_critical (_("%s(): fread() failed when reading recordsize"), __func__);
    return RIFIUTI_ERR_OPEN_FILE;
  }
  *recordsize = GUINT32_FROM_LE (*recordsize);

  /* Recordsize should be restricted to either 280 (v4) or 800 bytes (v5) */
  switch (*info2_version) 
  {
    case FORMAT_WIN98:
      if (*recordsize != VERSION4_RECORD_SIZE)
      {
        g_debug ("Size per record = %u, expect %u instead.", *recordsize, VERSION4_RECORD_SIZE);
        g_critical (_("Invalid record size for this version of INFO2"));
        return RIFIUTI_ERR_BROKEN_FILE;
      }
      if ( !legacy_encoding )
      {
        g_printerr (_("This INFO2 file was produced on a Windows 98. Please specify codepage "
                      "of concerned system with '-l' or '--legacy-filename' option.\n\n"));
        /* TRANSLATOR COMMENT: use suitable example from YOUR language & code page */
        g_printerr (_("For example, if file name was expected to contain accented latin characters, "
              "use '-l CP1252' option; or in case of Japanese characters, "
              "'-l CP932'.\n\n"
              "Code pages (or any other encodings) supported by 'iconv' can be used.\n"));
        return RIFIUTI_ERR_ARG;
      }
      break;

    case FORMAT_WIN2K:
      if (*recordsize != VERSION5_RECORD_SIZE)
      {
        g_debug ("Size per record = %u, expect %u instead.", *recordsize, VERSION5_RECORD_SIZE);
        g_critical (_("Invalid record size for this version of INFO2"));
        return RIFIUTI_ERR_BROKEN_FILE;
      }
      /* only version 5 contains UTF-16 filename */
      has_unicode_filename = TRUE;
      break;

    default:
      g_critical (_("It is not a supported INFO2 file, or probably not an INFO2 file."));
      return RIFIUTI_ERR_BROKEN_FILE;
  }
  return 0;
}


int main (int argc, char **argv)
{
  void           *buf;
  FILE           *infile, *outfile;
  int             status;
  GOptionGroup   *textoptgroup;
  GOptionContext *context;
  GError         *error = NULL;

  GStatBuf        st;
  rbin_struct    *record;
  uint32_t        recordsize, info2_version;
  uint64_t        win_filetime;
  time_t          file_epoch;
  char           *bug_report_str;

  unsigned char driveletters[28] =
  {
    'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U',
    'V', 'W', 'X', 'Y', 'Z', '\\', '?'
  };

  setlocale (LC_ALL, "");
  /* searching current dir might be more useful on e.g. Windows */
  if (g_file_test (LOCALEDIR, G_FILE_TEST_IS_DIR))
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  else
    bindtextdomain (GETTEXT_PACKAGE, ".");
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_option_context_new ("INFO2");
  g_option_context_set_summary (context,
      _("Parse INFO2 file and dump recycle bin data."));
  bug_report_str = g_strdup_printf (_("Report bugs to %s"), PACKAGE_BUGREPORT);
  g_option_context_set_description (context, bug_report_str);
  g_option_context_add_main_entries (context, mainoptions, "rifiuti");

  textoptgroup = g_option_group_new ("text", _("Plain text output options:"),
                                     N_("Show plain text output options"), NULL, NULL);
  g_option_group_set_translation_domain (textoptgroup, GETTEXT_PACKAGE);
  g_option_group_add_entries (textoptgroup, textoptions);
  g_option_context_add_group (context, textoptgroup);

  /* Must be done before parsing arguments since argc will be modified later */
  if (argc <= 1)
  {
    char *msg = g_option_context_get_help (context, FALSE, NULL);
#ifdef G_OS_WIN32
    g_set_print_handler (gui_message);
#endif
    g_print ("%s", msg);
    g_free (msg);
    g_option_context_free (context);
    exit (EXIT_SUCCESS);
  }

  g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, my_debug_handler, NULL);

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
    g_warning (_("Must specify exactly one INFO2 file as argument."));
    g_warning (_("Run program with '-?' option for more info."));
    exit (RIFIUTI_ERR_ARG);
  }

  if (outfilename)
  {
    outfile = g_fopen (outfilename, "wb");
    if (NULL == outfile)
    {
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
      g_critical (_("Plain text format options can not be used in XML mode."));
      exit (RIFIUTI_ERR_ARG);
    }
  }

  /* Is charset valid? */
  if (legacy_encoding)
  {
    GIConv try;
    try = g_iconv_open (legacy_encoding, "UTF-8");
    if (try == (GIConv) -1)
    {
      g_printerr (_("'%s' is not a valid code page or encoding. Only those supported by"
            " 'iconv' can be used.\n"), legacy_encoding);
#ifdef G_OS_WIN32
      g_printerr (_("Please visit following web page for a list closely resembling encodings supported by rifiuti:\n\n\t%s\n\n"),
          "https://www.gnu.org/software/libiconv/");
#endif
#ifdef G_OS_UNIX
      g_printerr (_("Please execute 'iconv -l' for list of supported encodings.\n"));
#endif
      exit (RIFIUTI_ERR_ENCODING);
    }
    else
      g_iconv_close (try);
  }

  if (NULL == delim)
    delim = g_strndup ("\t", 2);

  g_debug ("Start basic file checking...");

  if (!g_file_test (fileargs[0], G_FILE_TEST_EXISTS))
  {
    g_critical (_("'%s' does not exist."), fileargs[0]);
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  if (!g_file_test (fileargs[0], G_FILE_TEST_IS_REGULAR))
  {
    g_critical (_("'%s' is not a regular file."), fileargs[0]);
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  if ( 0 != g_stat (fileargs[0], &st) )
  {
    g_warning (_("Error getting metadata of file '%s': %s"), fileargs[0], strerror (errno));
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  if ( !( infile = g_fopen (fileargs[0], "rb") ) )
  {
    g_critical (_("Error opening file '%s' for reading: %s"), fileargs[0], strerror (errno));
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  status = validate_index_file (infile, st.st_size, &info2_version, &recordsize);
  if (0 != status)
  {
    fclose (infile);
    exit (status);
  }

  rewind (infile);
  if ( !no_heading || (output_format != OUTPUT_CSV) )
    print_header (outfile, fileargs[0], info2_version, TRUE);

  buf = g_malloc0 (recordsize);
  record = g_malloc0 (sizeof (rbin_struct));

  fseek (infile, RECORD_START_OFFSET, SEEK_SET);
  while (TRUE)
  {
    status = fread (buf, recordsize, 1, infile);
    if (status != 1)
    {
      if ( !feof (infile) )
        g_warning (_("Failed to read next record: %s"), strerror (errno));
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
        g_warning (_("Invalid drive letter (0x%X) for record %u."),
                   record->drive, record->index);
      }

      /* TODO: Safer handling of reading legacy filename */
      record->legacy_filename = (char *) g_malloc0 (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);
      g_snprintf (record->legacy_filename, RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET,
                  "%c%s", driveletters[record->drive],
                  (char *) (buf + LEGACY_FILENAME_OFFSET + 1));
    }

    /* File deletion time */
    memcpy (&win_filetime, buf + FILETIME_OFFSET, 8);
    win_filetime = GUINT64_FROM_LE (win_filetime);
    file_epoch = win_filetime_to_epoch (win_filetime);
    record->filetime = use_localtime ? localtime (&file_epoch) : gmtime (&file_epoch);

    /* File size or occupied cluster size */
    memcpy (&record->filesize, buf + FILESIZE_OFFSET, 4);
    record->filesize = GUINT32_FROM_LE (record->filesize);

    /* TODO: safer handling of reading junk after string */
    if (has_unicode_filename)
    {
      record->utf8_filename = g_utf16_to_utf8 ((gunichar2 *) (buf + UNICODE_FILENAME_OFFSET),
                                               WIN_PATH_MAX, NULL, NULL, &error);
      /* not checking error, since Windows <= 2000 may insert junk after UTF-16 file name */
      if (!record->utf8_filename)
      {
        g_warning (_("Error converting file name from %s encoding to UTF-8 for record %u: %s"),
                   "UTF-16", record->index, error->message);
        g_error_free (error);
        record->utf8_filename = g_strdup (_("(File name not representable in UTF-8 encoding)"));
      }
    }
    else
      record->utf8_filename = NULL;

    print_record (outfile, record);

    g_free (record->utf8_filename);
    g_free (record->legacy_filename);
  }

  print_footer (outfile);

  g_debug ("Cleaning up...");

  fclose (infile);
  fclose (outfile);

  g_free (record);
  g_free (buf);
  g_free (bug_report_str);

  exit (EXIT_SUCCESS);
}

/* vim: set sw=2 expandtab ts=2 : */
