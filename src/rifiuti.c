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
#ifdef G_OS_WIN32
#  include "utils-win.h"
#endif

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
static gboolean   do_print_version     = FALSE;
static int        exit_status          = EXIT_SUCCESS;
static metarecord meta;

/* 0-25 => A-Z, 26 => '\', 27 or above is erraneous */
unsigned char   driveletters[28] =
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U',
	'V', 'W', 'X', 'Y', 'Z', '\\', '?'
};

static GOptionEntry mainoptions[] =
{
	{"output", 'o', 0, G_OPTION_ARG_FILENAME, &outfilename,
	 N_("Write output to FILE"),
	 N_("FILE")},
	{"xml", 'x', 0, G_OPTION_ARG_NONE, &xml_output,
	 N_("Output in XML format instead of tab-delimited values"), NULL},
	{"legacy-filename", 'l', 0, G_OPTION_ARG_STRING, &legacy_encoding,
	 N_("Show legacy (8.3) filename if available and specify its CODEPAGE"),
	 N_("CODEPAGE")},
	{"localtime", 'z', 0, G_OPTION_ARG_NONE, &use_localtime,
	 N_("Present deletion time in time zone of local system (default is UTC)"),
	 NULL},
	{"version", 'v', 0, G_OPTION_ARG_NONE, &do_print_version,
	 N_("Print version information and exit"), NULL},
	{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
	 N_("INFO2 file name"), NULL},
	{NULL}
};

static GOptionEntry textoptions[] =
{
	{"delimiter", 't', 0, G_OPTION_ARG_STRING, &delim,
	 N_("String to use as delimiter (TAB by default)"), N_("STRING")},
	{"no-heading", 'n', 0, G_OPTION_ARG_NONE, &no_heading,
	 N_("Don't show header info"), NULL},
	{"always-utf8", '8', 0, G_OPTION_ARG_NONE, &always_utf8,
	 N_("Always display result in UTF-8 encoding"), NULL},
	{NULL}
};


/*
 * Check if index file has sufficient amount of data for reading
 * 0 = success, all other return status = error
 * If success, infile will be set to file pointer and other args
 * will be filled, otherwise file pointer = NULL
 */
static int
validate_index_file (const char  *filename,
                     FILE       **infile,
                     metarecord  *meta)
{
	size_t          status;
	GStatBuf        st;
	FILE           *fp = NULL;
	uint32_t        ver, size;

	g_debug ("Start file validation...");

	g_return_val_if_fail ( (infile != NULL), RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (meta   != NULL), RIFIUTI_ERR_INTERNAL );
	*infile = NULL;

	if (0 != g_stat (filename, &st))
	{
		g_printerr (_("Error getting metadata of file '%s': %s\n"), filename,
		            strerror (errno));
		return RIFIUTI_ERR_OPEN_FILE;
	}

	if (st.st_size < RECORD_START_OFFSET)  /* empty INFO2 file has 20 bytes */
	{
		g_debug ("file size = %d, expect at least %d\n", (int) st.st_size,
		         RECORD_START_OFFSET);
		return RIFIUTI_ERR_BROKEN_FILE;
	}

	if ( !(fp = g_fopen (filename, "rb")) )
	{
		g_printerr (_("Error opening file '%s' for reading: %s\n"), filename,
		            strerror (errno));
		return RIFIUTI_ERR_OPEN_FILE;
	}

	/* with file size check already done, fread fail -> serious problem */
	status = fread (&ver, sizeof (ver), 1, fp);
	if (status < 1)
	{
		/* TRANSLATOR COMMENT: the variable is function name */
		g_critical (_("%s(): fread() failed when reading version info from '%s'"),
		            __func__, filename);
		status = RIFIUTI_ERR_OPEN_FILE;
		goto validation_broken;
	}
	ver = GUINT32_FROM_LE (ver);

	fseek (fp, RECORD_SIZE_OFFSET, SEEK_SET);
	status = fread (&size, sizeof (size), 1, fp);
	if (status < 1)
	{
		/* TRANSLATOR COMMENT: the variable is function name */
		g_critical (_("%s(): fread() failed when reading recordsize from '%s'"),
		            __func__, filename);
		status = RIFIUTI_ERR_OPEN_FILE;
		goto validation_broken;
	}
	size = GUINT32_FROM_LE (size);

	/* Turns out version is not reliable indicator. Use size instead */
	switch (size)
	{
	  case LEGACY_RECORD_SIZE:

		/* Windows ME still use 280 byte record */
		if ( ( ver != VERSION_ME_03 ) &&
		     ( ver != VERSION_WIN98 ) &&
		     ( ver != VERSION_WIN95 ) )
		{
			g_printerr (_("File is not supported, or it is probably not an "
	              "INFO2 file.\n"));
			status = RIFIUTI_ERR_BROKEN_FILE;
			goto validation_broken;
		}

		/* No version check; this size can be used in all versions */
		if (!legacy_encoding)
		{
			g_printerr (_("This INFO2 file was produced on a legacy system "
			              "without Unicode file name (Windows ME or earlier). "
			              "Please specify codepage of concerned system with "
			              "'-l' or '--legacy-filename' option.\n\n"));
			/* TRANSLATOR COMMENT: use suitable example from YOUR language & code page */
			g_printerr (_("For example, if file name was expected to contain "
			              "accented latin characters, use '-l CP1252' option; "
			              "or in case of Japanese characters, '-l CP932'.\n\n"
			              "Code pages (or any other encodings) supported by "
			              "'iconv' can be used.\n"));
			status = RIFIUTI_ERR_ARG;
			goto validation_broken;
		}
		break;

	  case UNICODE_RECORD_SIZE:
		if ( ( ver != VERSION_ME_03 ) && ( ver != VERSION_NT4 ) )
		{
			g_printerr (_("File is not supported, or it is probably not an "
	              "INFO2 file.\n"));
			status = RIFIUTI_ERR_BROKEN_FILE;
			goto validation_broken;
		}
		has_unicode_filename = TRUE;
		break;

	  default:
		status = RIFIUTI_ERR_BROKEN_FILE;
		goto validation_broken;
	}

	rewind (fp);
	*infile = fp;
	meta->version = (int64_t) ver;
	meta->recordsize = size;

	return EXIT_SUCCESS;

  validation_broken:
	fclose (fp);
	return status;
}


static rbin_struct *
populate_record_data (void *buf)
{
	rbin_struct    *record;
	uint64_t        win_filetime;
	uint32_t        drivenum;
	long            read, write;

	record = g_malloc0 (sizeof (rbin_struct));

	/* Guarantees null-termination by allocating extra byte */
	record->legacy_filename =
		(char *) g_malloc0 (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET + 1);
	copy_field (record->legacy_filename, LEGACY_FILENAME, RECORD_INDEX);

	copy_field (&record->index_n, RECORD_INDEX, DRIVE_LETTER);
	record->index_n = GUINT32_FROM_LE (record->index_n);
	g_debug ("index=%u", record->index_n);

	copy_field (&drivenum, DRIVE_LETTER, FILETIME);
	drivenum = GUINT32_FROM_LE (drivenum);
	g_debug ("drive=%u", drivenum);
	if (drivenum >= sizeof (driveletters) - 1)
		g_warning (_("Invalid drive number (0x%X) for record %u."),
		           drivenum, record->index_n);
	record->drive = driveletters[MIN (drivenum, sizeof (driveletters) - 1)];

	record->emptied = FALSE;
	/* first byte will be removed from filename if file is not in recycle bin */
	if (!*record->legacy_filename)
	{
		record->emptied = TRUE;
		*record->legacy_filename = record->drive;
	}

	/* File deletion time */
	copy_field (&win_filetime, FILETIME, FILESIZE);
	win_filetime = GUINT64_FROM_LE (win_filetime);
	record->deltime = win_filetime_to_epoch (win_filetime);

	/* File size or occupied cluster size */
	/* BEWARE! This is 32bit data casted to 64bit struct member */
	copy_field (&record->filesize, FILESIZE, UNICODE_FILENAME);
	record->filesize = GUINT64_FROM_LE (record->filesize);
	g_debug ("filesize=%" G_GUINT64_FORMAT, record->filesize);

	if (has_unicode_filename)
	{
		GError *error = NULL;
		/*
		 * Added safeguard to memory buffer (2 bytes larger than necessary),
		 * so safely assume string is null terminated
		 */
		record->utf8_filename =
			utf16le_to_utf8 ((gunichar2 *) (buf + UNICODE_FILENAME_OFFSET),
			                 WIN_PATH_MAX + 1, &read, &write, &error);
		g_debug ("utf16->8 r=%li w=%li", read, write);

		if (error)
		{
			g_warning (_("Error converting file name from %s encoding to "
			             "UTF-8 encoding for record %u: %s"),
			           "UTF-16", record->index_n, error->message);
			g_clear_error (&error);
		}
	}
	return record;
}


static void
parse_record (char    *index_file,
              GSList **recordlist)
{
	rbin_struct *record;
	FILE        *infile;
	size_t       size;
	void        *buf = NULL;

	exit_status = validate_index_file (index_file, &infile, &meta);
	if ( exit_status != EXIT_SUCCESS )
	{
		g_printerr (_("File '%s' fails validation.\n"), index_file);
		return;
	}

	g_debug ("Start populating record for '%s'...", index_file);

	/*
	 * Add 2 padding bytes as null-termination of unicode file name.
	 * Not so confident that file names created with Win2K or earlier
	 * are null terminated, because random memory fragments are copied
	 * to the padding bytes.
	 */
	buf = g_malloc0 (meta.recordsize + 2);

	fseek (infile, RECORD_START_OFFSET, SEEK_SET);

	meta.is_empty = TRUE;  /* EOF flag is cleared by fseek */
	while (meta.recordsize == (size = fread (buf, 1, meta.recordsize, infile)))
	{
		record = populate_record_data (buf);
		record->meta = &meta;
		/* INFO2 already sort entries by time */
		*recordlist = g_slist_append (*recordlist, record);
		meta.is_empty = FALSE;
	}
	g_free (buf);

	if ( ferror (infile) )
	{
		g_critical (_("Failed to read record at position %li: %s"),
				   ftell (infile), strerror (errno));
		exit_status = RIFIUTI_ERR_OPEN_FILE;
	}
	if ( feof (infile) && size && ( size < meta.recordsize ) )
	{
		g_printerr (_("Premature end of file, last record (%zu bytes) discarded\n"), size);
		exit_status = RIFIUTI_ERR_BROKEN_FILE;
	}

	fclose (infile);
}


int
main (int    argc,
      char **argv)
{
	FILE           *outfile;
	GSList         *filelist = NULL;
	GSList         *recordlist = NULL;
	char           *tmppath = NULL;
	GOptionContext *context;

	rifiuti_init (argv[0]);

	context = g_option_context_new ("INFO2");
	g_option_context_set_summary
		(context, _("Parse INFO2 file and dump recycle bin data."));
	rifiuti_setup_opt_ctx (&context, mainoptions, textoptions);
	exit_status = rifiuti_parse_opt_ctx (&context, &argc, &argv);
	if ( EXIT_SUCCESS != exit_status )
		goto cleanup;

	if (do_print_version)
	{
		print_version();
		goto cleanup;
	}

	if (!fileargs || g_strv_length (fileargs) > 1)
	{
		g_printerr (_("Must specify exactly one INFO2 file as argument.\n\n"));
		g_printerr (_("Run program with '-?' option for more info.\n"));
		exit_status = RIFIUTI_ERR_ARG;
		goto cleanup;
	}

	if (xml_output)
	{
		output_format = OUTPUT_XML;
		if (no_heading || always_utf8 || (NULL != delim))
		{
			g_printerr (_("Plain text format options "
			              "can not be used in XML mode.\n"));
			exit_status = RIFIUTI_ERR_ARG;
			goto cleanup;
		}
	}

	/* Is charset valid? */
	if (legacy_encoding)
	{
		GIConv try;
		try = g_iconv_open (legacy_encoding, "UTF-8");
		if (try == (GIConv) - 1)
		{
			g_printerr (_("'%s' is not a valid code page or encoding. "
			              "Only those supported by 'iconv' can be used.\n"),
					legacy_encoding);
#ifdef G_OS_WIN32
			g_printerr (_("Please visit following web page for a list "
			              "closely resembling encodings supported by "
			              "rifiuti:\n\n\t%s\n\n"),
					"https://www.gnu.org/software/libiconv/");
#endif
#ifdef G_OS_UNIX
			g_printerr (_("Please execute 'iconv -l' for list "
			              "of supported encodings.\n"));
#endif
			exit_status = RIFIUTI_ERR_ARG;
			goto cleanup;
		}
		else
			g_iconv_close (try);
	}

	if (NULL == delim)
		delim = g_strndup ("\t", 2);
	else
	{
		char *d = filter_escapes (delim);
		if (d != NULL)
		{
			g_free (delim);
			delim = d;
		}
	}

	exit_status = check_file_args (fileargs[0], &filelist, TRUE);
	if ( EXIT_SUCCESS != exit_status )
		goto cleanup;

	/*
	 * TODO May be silly for single file, but would be useful in future
	 * when reading multiple files from live system
	 */
	g_slist_foreach (filelist, (GFunc) parse_record, &recordlist);

	/* Fill in recycle bin metadata */
	meta.type     = RECYCLE_BIN_TYPE_FILE;
	meta.filename = fileargs[0];
	/* Keeping info for deleted entry is only available since 98 */
	meta.keep_deleted_entry = ( meta.version >= VERSION_WIN98 );
	meta.os_guess = NULL;    /* TODO */

	if ( !meta.is_empty && (recordlist == NULL) )
	{
		g_printerr ("%s", _("Recycle bin file has no valid record.\n"));
		exit_status = RIFIUTI_ERR_BROKEN_FILE;
		goto cleanup;
	}

	if (outfilename)
	{
		int tmpfile;
		tmppath = g_build_filename (g_get_tmp_dir(), "rifiuti-XXXXXX", NULL);
		if ( ( -1 == (tmpfile = g_mkstemp (tmppath)) ) ||
		     ( NULL == (outfile = fdopen (tmpfile, "wb")) ) )
		{
			g_printerr (_("Error opening temp file for writing: %s\n"),
			            strerror (errno) );
			exit_status = RIFIUTI_ERR_OPEN_FILE;
			goto cleanup;
		}
	}
	else
		outfile = stdout;

	/* Print everything */
	if (!no_heading)
		print_header (outfile, meta);
	g_slist_foreach (recordlist, (GFunc) print_record, outfile);
	print_footer (outfile);

	fclose (outfile);

	if ( ( outfile != stdout ) && ( -1 == g_rename (tmppath, outfilename) ) )
	{
		/* TRANSLATOR COMMENT: arg 1 is err message, 2nd is temp file
		 * location when failed to be moved to proper location */
		g_printerr (_("Error moving output data to desinated file: %s\n"
					"Output content is left in '%s'.\n"),
				strerror(errno), tmppath);
		exit_status = RIFIUTI_ERR_WRITE_FILE;
	}

  cleanup:
	g_debug ("Cleaning up...");

	g_free (tmppath);

	/* g_slist_free_full() available only since 2.28 */
	g_slist_foreach (recordlist, (GFunc) free_record, NULL);
	g_slist_free (recordlist);

	g_slist_foreach (filelist, (GFunc) g_free, NULL);
	g_slist_free (filelist);

	g_strfreev (fileargs);
	g_free (outfilename);
	g_free (legacy_encoding);
	g_free (delim);

	return exit_status;
}

/* vim: set sw=4 ts=4 noexpandtab : */
