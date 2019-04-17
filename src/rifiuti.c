/* vim: set sw=4 ts=4 noexpandtab : */
/*
 * Copyright (C) 2003, by Keith J. Jones.
 * Copyright (C) 2007-2019 Abel Cheung.
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


       FILE      *out_fh               = NULL;
       char      *delim                = NULL;
static char     **fileargs             = NULL;
static char      *outfilename          = NULL;
       char      *legacy_encoding      = NULL;
       int        output_format        = OUTPUT_CSV;
static gboolean   no_heading           = FALSE;
static gboolean   xml_output           = FALSE;
       gboolean   always_utf8          = FALSE;
       gboolean   use_localtime        = FALSE;
static gboolean   do_print_version     = FALSE;
static r2status   exit_status          = EXIT_SUCCESS;
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
	 N_("(This option is deprecated)"), NULL},
	{NULL}
};


/*
 * Check if index file has sufficient amount of data for reading
 * 0 = success, all other return status = error
 * If success, infile will be set to file pointer and other args
 * will be filled, otherwise file pointer = NULL
 */
static r2status
validate_index_file (const char  *filename,
                     FILE       **infile)
{
	size_t          status;
	GStatBuf        st;
	FILE           *fp = NULL;
	uint32_t        ver, size;

	g_debug ("Start file validation...");

	g_return_val_if_fail ( (infile != NULL), R2_ERR_INTERNAL );
	*infile = NULL;

	if (0 != g_stat (filename, &st))
	{
		g_printerr (_("Error getting metadata of file '%s': %s"),
			filename, strerror (errno));
		g_printerr ("\n");
		return R2_ERR_OPEN_FILE;
	}

	if (st.st_size < RECORD_START_OFFSET)  /* empty INFO2 file has 20 bytes */
	{
		g_debug ("file size = %d, expect at least %d\n", (int) st.st_size,
		         RECORD_START_OFFSET);
		return R2_ERR_BROKEN_FILE;
	}

	if ( !(fp = g_fopen (filename, "rb")) )
	{
		g_printerr (_("Error opening file '%s' for reading: %s"),
			filename, strerror (errno));
		g_printerr ("\n");
		return R2_ERR_OPEN_FILE;
	}

	/* with file size check already done, fread fail -> serious problem */
	status = fread (&ver, sizeof (ver), 1, fp);
	if (status < 1)
	{
		/* TRANSLATOR COMMENT: the variable is function name */
		g_critical (_("%s(): fread() failed when reading version info from '%s'"),
		            __func__, filename);
		status = R2_ERR_OPEN_FILE;
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
		status = R2_ERR_OPEN_FILE;
		goto validation_broken;
	}
	size = GUINT32_FROM_LE (size);

	/* Turns out version is not reliable indicator. Use size instead */
	switch (size)
	{
	  case LEGACY_RECORD_SIZE:

		meta.has_unicode_path = FALSE;

		if ( ( ver != VERSION_ME_03 ) &&
		     ( ver != VERSION_WIN98 ) &&
		     ( ver != VERSION_WIN95 ) )  /* Windows ME still use 280 byte record */
		{
			g_printerr (_("Unsupported file version, or probably not an INFO2 file at all."));
			g_printerr ("\n");
			status = R2_ERR_BROKEN_FILE;
			goto validation_broken;
		}

		if (!legacy_encoding)
		{
			g_printerr (_("This INFO2 file was produced on a legacy system "
			              "without Unicode file name (Windows ME or earlier). "
			              "Please specify codepage of concerned system with "
			              "'-l' or '--legacy-filename' option."));
			g_printerr ("\n\n");
			/* TRANSLATOR COMMENT: can choose example from YOUR language & code page */
			g_printerr (_("For example, if recycle bin is expected to come from West "
			              "European versions of Windows, use '-l CP1252' option; "
			              "or in case of Japanese Windows, use '-l CP932'."));
			g_printerr ("\n\n");
#ifdef G_OS_UNIX
			g_printerr (_("Code pages supported by 'iconv' can be used."));
			g_printerr ("\n");
#endif

			status = R2_ERR_ARG;
			goto validation_broken;
		}

		switch (ver)
		{
		  case VERSION_WIN95:
			meta.os_guess = OS_GUESS_95; break;
		  case VERSION_WIN98:
			meta.os_guess = OS_GUESS_98; break;
		  case VERSION_ME_03:
			meta.os_guess = OS_GUESS_ME; break;
		}

		break;

	  case UNICODE_RECORD_SIZE:

		meta.has_unicode_path = TRUE;
		if ( ( ver != VERSION_ME_03 ) && ( ver != VERSION_NT4 ) )
		{
			g_printerr (_("Unsupported file version, or probably not an INFO2 file at all."));
			g_printerr ("\n");
			status = R2_ERR_BROKEN_FILE;
			goto validation_broken;
		}
		/* guess is not complete yet for latter case, see populate_record_data */
		meta.os_guess = (ver == VERSION_NT4) ? OS_GUESS_NT4 : OS_GUESS_2K_03;
		break;

	  default:
		status = R2_ERR_BROKEN_FILE;
		goto validation_broken;
	}

	rewind (fp);
	*infile = fp;
	meta.version = (int64_t) ver;
	meta.recordsize = size;

	return EXIT_SUCCESS;

  validation_broken:
	fclose (fp);
	return status;
}


static rbin_struct *
populate_record_data (void *buf)
{
	rbin_struct *record;
	uint64_t     win_filetime;
	uint32_t     drivenum;
	size_t       read;
	char        *legacy_fname;

	record = g_malloc0 (sizeof (rbin_struct));

	/* Guarantees null-termination by allocating extra byte; same goes with
	 * unicode filename */
	legacy_fname = g_malloc0 (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET + 1);
	copy_field (legacy_fname, LEGACY_FILENAME, RECORD_INDEX);

	/* Index number associated with the record */
	copy_field (&record->index_n, RECORD_INDEX, DRIVE_LETTER);
	record->index_n = GUINT32_FROM_LE (record->index_n);
	g_debug ("index=%u", record->index_n);

	/* Number representing drive letter */
	copy_field (&drivenum, DRIVE_LETTER, FILETIME);
	drivenum = GUINT32_FROM_LE (drivenum);
	g_debug ("drive=%u", drivenum);
	if (drivenum >= sizeof (driveletters) - 1)
		g_warning (_("Invalid drive number (0x%X) for record %u."),
		           drivenum, record->index_n);
	record->drive = driveletters[MIN (drivenum, sizeof (driveletters) - 1)];

	record->emptied = FALSE;
	/* first byte will be removed from filename if file is not in recycle bin */
	if (!*legacy_fname)
	{
		record->emptied = TRUE;
		*legacy_fname = record->drive;
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

	/*
	 * 1. Only bother populating legacy path if users need it,
	 *    because otherwise we don't know which encoding to use
	 * 2. Enclose with angle brackets because they are not allowed
	 *    in Windows file name, therefore stands out better that
	 *    the escaped hex sequences are not part of real file name
	 */
	if (legacy_encoding)
	{
		record->legacy_filename = conv_path_to_utf8_with_tmpl (
			legacy_fname, legacy_encoding, "<\\%02X>", &read, &exit_status);

		if (record->legacy_filename == NULL) {
			g_warning (_("(Record %u) Error converting legacy path to UTF-8."),
				record->index_n);
			record->legacy_filename = "";
		}
	}

	g_free (legacy_fname);

	if (! meta.has_unicode_path)
		return record;

	/*******************************************
	 * Part below deals with unicode path only *
	 *******************************************/

	record->uni_filename = conv_path_to_utf8_with_tmpl (
		(char *) (buf + UNICODE_FILENAME_OFFSET), NULL,
		"<\\u%04X>", &read, &exit_status);

	if (record->uni_filename == NULL) {
		g_warning (_("(Record %u) Error converting unicode path to UTF-8."),
			record->index_n);
		record->uni_filename = "";
	}

	/*
	 * We check for junk memory filling the padding area after
	 * unicode path, using it as the indicator of OS generating this
	 * INFO2 file. (server 2000 / 2003)
	 *
	 * The padding area after legacy path is no good; experiment
	 * shows that legacy path *always* contain non-zero bytes after
	 * null terminator if path contains double-byte character,
	 * regardless of OS.
	 *
	 * Those non-zero bytes resemble partial end of full path.
	 * Looks like an ANSI codepage full path is filled in
	 * legacy path field, then overwritten in place by a 8.3
	 * version of path whenever applicable (which was always shorter).
	 */
	if (! meta.fill_junk)
	{
		void *ptr;

		for (ptr = buf + UNICODE_FILENAME_OFFSET + read;
			ptr < buf + UNICODE_RECORD_SIZE; ptr++)
		{
			if ( *(char *) ptr != '\0' )
			{
				g_debug ("Junk detected at offset 0x%tx of unicode path",
					ptr - buf - UNICODE_FILENAME_OFFSET);
				meta.fill_junk = TRUE;
				break;
			}
		}
	}

	return record;
}


static void
parse_record_cb (char    *index_file,
                 GSList **recordlist)
{
	rbin_struct *record;
	FILE        *infile;
	size_t       size;
	void        *buf = NULL;

	exit_status = validate_index_file (index_file, &infile);
	if ( exit_status != EXIT_SUCCESS )
	{
		g_printerr (_("File '%s' fails validation."), index_file);
		g_printerr ("\n");
		return;
	}

	g_debug ("Start populating record for '%s'...", index_file);

	/*
	 * Add padding bytes as null-termination of unicode file name.
	 * Normally Windows should have done the null termination within
	 * WIN_PATH_MAX limit, but on 98/ME/2000 programmers were sloppy
	 * and use junk memory as padding, so just play safe.
	 */
	buf = g_malloc0 (meta.recordsize + sizeof(gunichar2));

	fseek (infile, RECORD_START_OFFSET, SEEK_SET);

	meta.is_empty = TRUE;  /* no feof; EOF flag cleared by fseek */
	while (meta.recordsize == (size = fread (buf, 1, meta.recordsize, infile)))
	{
		record = populate_record_data (buf);
		record->meta = &meta;
		/* INFO2 already sort entries by time */
		*recordlist = g_slist_append (*recordlist, record);
		meta.is_empty = FALSE;
	}
	g_free (buf);

	/* do this only when all entries are scanned */
	if ((!meta.is_empty) && (meta.os_guess == OS_GUESS_2K_03))
		meta.os_guess = meta.fill_junk ? OS_GUESS_2K : OS_GUESS_XP_03;

	if ( ferror (infile) )
	{
		g_critical (_("Failed to read record at position %li: %s"),
				   ftell (infile), strerror (errno));
		exit_status = R2_ERR_OPEN_FILE;
	}
	if ( feof (infile) && size && ( size < meta.recordsize ) )
	{
		g_warning (_("Premature end of file, last record (%zu bytes) discarded"), size);
		exit_status = R2_ERR_BROKEN_FILE;
	}

	fclose (infile);
}

static r2status
_check_legacy_encoding (const char *enc)
{
	char     *s;
	GError   *err = NULL;
	r2status  st  = EXIT_SUCCESS;

	if (enc == NULL)
		return EXIT_SUCCESS;

	s = g_convert ("C:\\", -1, "UTF-8", enc, NULL, NULL, &err);

	if (err != NULL)
	{
		int e = err->code;
		g_clear_error (&err);

		switch (e)
		{
			case G_CONVERT_ERROR_NO_CONVERSION:

				st = R2_ERR_ARG;

				g_printerr (_("'%s' is not a valid encoding on this system."), enc);
				g_printerr ("\n");
#ifdef G_OS_WIN32
				/* TRANSLATOR COMMENT: argument is software name, 'rifiuti2' */
				g_printerr (_("Please visit following web page for a list "
					"closely resembling encodings supported by %s:"), PACKAGE);

				g_printerr ("\n\n\t%s\n", "https://www.gnu.org/software/libiconv/");
#endif
#ifdef G_OS_UNIX
				g_printerr (_("Run 'iconv -l' for list of supported encodings."));
				g_printerr ("\n");
#endif
				break;

			/* Encodings not ASCII compatible can't possibly be ANSI/OEM code pages */
			case G_CONVERT_ERROR_ILLEGAL_SEQUENCE:
			case G_CONVERT_ERROR_PARTIAL_INPUT:

				st = R2_ERR_ARG;

				g_printerr (_("'%s' can't possibly be a code page or compatible encoding "
					"used by localized Windows."), enc);
				g_printerr ("\n");

				break;

			default:
				g_assert_not_reached ();
		}
	} else if (strcmp ("C:\\", s) != 0) {	/* Can happen for EBCDIC based code pages */
		st = R2_ERR_ARG;
		g_printerr (_("'%s' can't possibly be a code page or compatible encoding "
			"used by localized Windows."), enc);
		g_printerr ("\n");
	}

	g_free (s);
	return st;
}

int
main (int    argc,
      char **argv)
{
	GSList         *filelist   = NULL;
	GSList         *recordlist = NULL;
	char           *tmppath    = NULL;
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
		g_printerr (_("Must specify exactly one INFO2 file as argument."));
		g_printerr ("\n");
		g_printerr (_("Run program with '-h' option for more info."));
		g_printerr ("\n");
		exit_status = R2_ERR_ARG;
		goto cleanup;
	}

    if (always_utf8) {
        g_printerr (_("'-8' option is deprecated and ignored."));
		g_printerr ("\n");
	}

	if (xml_output)
	{
		output_format = OUTPUT_XML;
		if (no_heading || (NULL != delim))
		{
			g_printerr (_("Plain text format options can not be used in XML mode."));
			g_printerr ("\n");
			exit_status = R2_ERR_ARG;
			goto cleanup;
		}
	}

	exit_status = _check_legacy_encoding (legacy_encoding);
	if (exit_status != EXIT_SUCCESS)
		goto cleanup;

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

	/* To be overwritten in parse_record_cb() when appropriate */
	meta.os_guess = OS_GUESS_UNKNOWN;

	/*
	 * TODO May be silly for single file, but would be useful in future
	 * when reading multiple files from live system
	 */
	g_slist_foreach (filelist, (GFunc) parse_record_cb, &recordlist);

	meta.type     = RECYCLE_BIN_TYPE_FILE;
	meta.filename = fileargs[0];
	/*
	 * Keeping deleted entry is only available since 98
	 * Note: always set this variable after parse_record_cb() because
	 * meta.version is not set beforehand
	 */
	meta.keep_deleted_entry = ( meta.version >= VERSION_WIN98 );

	if ( !meta.is_empty && (recordlist == NULL) )
	{
		g_printerr ("%s", _("Recycle bin file has no valid record.\n"));
		exit_status = R2_ERR_BROKEN_FILE;
		goto cleanup;
	}

	if (outfilename)
	{
		exit_status = get_tempfile (&out_fh, &tmppath);

		if (exit_status != EXIT_SUCCESS)
			goto cleanup;
	}
	else
	{
#ifdef G_OS_WIN32
		if (!init_wincon_handle())
#endif
			out_fh = stdout;
	}

	/* Print everything */
	if (!no_heading)
		print_header (meta);
	g_slist_foreach (recordlist, (GFunc) print_record_cb, NULL);
	print_footer ();

	if (out_fh != NULL)
		fclose (out_fh);
#ifdef G_OS_WIN32
	close_wincon_handle();
#endif

	/* file descriptor should have been closed at this point */
	if ( ( tmppath != NULL ) && ( -1 == g_rename (tmppath, outfilename) ) )
	{
		/* TRANSLATOR COMMENT: argument is system error message */
		g_printerr (_("Error moving output data to desinated file: %s"),
			strerror(errno));
		g_printerr ("\n");

		/* TRANSLATOR COMMENT: argument is temp file location, which
		 * failed to be moved to proper location */
		g_printerr (_("Output content is left in '%s'."), tmppath);
		g_printerr ("\n");

		exit_status = R2_ERR_WRITE_FILE;
	}

  cleanup:
	/* Last minute error messages for accumulated non-fatal errors */
	switch (exit_status)
	{
		case R2_ERR_USER_ENCODING:
		if (legacy_encoding) {
			g_printerr (_("Some entries could not be interpreted in %s encoding."
				"  The concerned characters are displayed in hex value instead."
				"  Very likely the (localised) Windows generating the recycle bin "
				"artifact does not use specified codepage."), legacy_encoding);
		} else {
			g_printerr (_("Some entries could not be presented as correct "
				"unicode path.  The concerned characters are displayed "
				"in escaped unicode sequences."));
		}
			g_printerr ("\n");
			break;

		default:
			break;
	}
	g_debug ("Cleaning up...");

	g_slist_free_full (recordlist, (GDestroyNotify) free_record_cb);
	g_slist_free_full (filelist  , (GDestroyNotify) g_free        );

	g_strfreev (fileargs);
	g_free (outfilename);
	g_free (legacy_encoding);
	g_free (delim);
	g_free (tmppath);

	return exit_status;
}
