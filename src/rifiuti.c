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
       char      *legacy_encoding      = NULL;
       int        output_format        = OUTPUT_CSV;
static gboolean   no_heading           = FALSE;
static gboolean   xml_output           = FALSE;
       gboolean   always_utf8          = FALSE;
       gboolean   has_unicode_filename = FALSE;
       gboolean   use_localtime        = FALSE;

/* 0-25 => A-Z, 26 => '\', 27 or above is erraneous */
unsigned char driveletters[28] =
{
  'A', 'B', 'C', 'D', 'E', 'F', 'G',
  'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U',
  'V', 'W', 'X', 'Y', 'Z', '\\', '?'
};

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
    g_printerr (_("This INFO2 file is truncated, or probably not an INFO2 file.\n"));
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
      g_printerr (_("It is not a supported INFO2 file, or probably not an INFO2 file.\n"));
      return RIFIUTI_ERR_BROKEN_FILE;
  }
  return 0;
}


static rbin_struct *populate_record_data (void *buf)
{
  rbin_struct *record;
  uint64_t     win_filetime;
  uint32_t     drivenum;

  record = g_malloc0 (sizeof (rbin_struct));
  record->type = RECYCLE_BIN_TYPE_FILE;

  /* Guarantees null-termination by allocating extra byte */
  record->legacy_filename =
    (char *) g_malloc0 (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET + 1);
  memcpy (record->legacy_filename, buf + LEGACY_FILENAME_OFFSET,
      RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);

  memcpy (&record->index_n, buf + RECORD_INDEX_OFFSET, sizeof(record->index_n));
  record->index_n = GUINT32_FROM_LE (record->index_n);

  memcpy (&drivenum, buf + DRIVE_LETTER_OFFSET, sizeof(drivenum));
  drivenum = GUINT32_FROM_LE (drivenum);
  if ( drivenum >= sizeof (driveletters) - 1 )
    g_warning (_("Invalid drive number (0x%X) for record %u."),
        drivenum, record->index_n);
  record->drive = driveletters[ MIN( drivenum, sizeof(driveletters)-1 )];

  record->emptied = FALSE;
  /* first byte will be removed from filename if file is not in recycle bin */
  if (!*record->legacy_filename)
  {
    record->emptied = TRUE;
    *record->legacy_filename = record->drive;
  }

  /* File deletion time */
  memcpy (&win_filetime, buf + FILETIME_OFFSET, 8);
  win_filetime = GUINT64_FROM_LE (win_filetime);
  record->deltime = win_filetime_to_epoch (win_filetime);

  /* File size or occupied cluster size */
  memcpy (&record->filesize, buf + FILESIZE_OFFSET, 4);
  record->filesize = GUINT32_FROM_LE (record->filesize);

  if (has_unicode_filename)
  {
    GError *error = NULL;

    /* Added safeguard to memory buffer (2 bytes larger than necessary), so safely assume
     * string is null terminated
     */
    record->utf8_filename = g_utf16_to_utf8 ((gunichar2 *) (buf + UNICODE_FILENAME_OFFSET),
                                             -1, NULL, NULL, &error);
    if (error)
    {
      g_warning (_("Error converting file name from %s encoding to UTF-8 for record %u: %s"),
                 "UTF-16", record->index_n, error->message);
      g_clear_error (&error);
      record->utf8_filename = g_strdup (_("(File name not representable in UTF-8 encoding)"));
    }
  }
  else
    record->utf8_filename = NULL;

  return record;
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
  char           *bug_report_str;

  /* displaying localized file names not working so well */
  g_setenv ("LC_CTYPE", "UTF-8", TRUE);
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
    g_free (bug_report_str);
    exit (EXIT_SUCCESS);
  }

  g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, my_debug_handler, NULL);

  {
    gboolean i;
    /* The user case where this code won't provide benefit is VERY rare,
     * so don't bother doing fallback because it never worked for them
     */
#if GLIB_CHECK_VERSION(2, 40, 0) && defined (G_OS_WIN32)
    char **args;

    args = g_win32_get_command_line();
    i = g_option_context_parse_strv (context, &args, &error);
    g_strfreev (args);
#else
    i = g_option_context_parse (context, &argc, &argv, &error);
#endif
    g_option_context_free (context);
    g_free (bug_report_str);

    if ( !i )
    {
      g_printerr (_("Error parsing options: %s\n"), error->message);
      g_clear_error (&error);
      exit (RIFIUTI_ERR_ARG);
    }
  }

  if ( !fileargs || g_strv_length (fileargs) > 1 )
  {
    g_printerr (_("Must specify exactly one INFO2 file as argument.\n\n"));
    g_printerr (_("Run program with '-?' option for more info.\n"));
    exit (RIFIUTI_ERR_ARG);
  }

  if (outfilename)
  {
    outfile = g_fopen (outfilename, "wb");
    if (NULL == outfile)
    {
      g_printerr (_("Error opening file '%s' for writing: %s\n"), outfilename, strerror (errno));
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
      g_printerr (_("Plain text format options can not be used in XML mode.\n"));
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
    g_printerr (_("'%s' does not exist.\n"), fileargs[0]);
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  if (!g_file_test (fileargs[0], G_FILE_TEST_IS_REGULAR))
  {
    g_printerr (_("'%s' is not a normal file.\n"), fileargs[0]);
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  if ( 0 != g_stat (fileargs[0], &st) )
  {
    g_printerr (_("Error getting metadata of file '%s': %s\n"), fileargs[0], strerror (errno));
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  if ( !( infile = g_fopen (fileargs[0], "rb") ) )
  {
    g_printerr (_("Error opening file '%s' for reading: %s\n"), fileargs[0], strerror (errno));
    exit (RIFIUTI_ERR_OPEN_FILE);
  }

  status = validate_index_file (infile, st.st_size, &info2_version, &recordsize);
  if ( status != 0 )
  {
    fclose (infile);
    exit (status);
  }

  rewind (infile);
  if ( !no_heading )
    print_header (outfile, fileargs[0], (int64_t)info2_version, TRUE);

  /* Add 2 padding bytes as null-termination of unicode file name. Not so confident
   * that file names created with Win2K or earlier are null terminated, because 
   * random memory fragments are copied to the padding bytes
   */
  buf = g_malloc0 (recordsize + 2);

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

    record = populate_record_data (buf);
    print_record (record, outfile);

    g_free (record->utf8_filename);
    g_free (record->legacy_filename);
    g_free (record);
  }

  print_footer (outfile);

  g_debug ("Cleaning up...");

  fclose (infile);
  fclose (outfile);

  g_free (buf);

  exit (EXIT_SUCCESS);
}

/* vim: set sw=2 expandtab ts=2 : */
