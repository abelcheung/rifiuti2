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
	 N_("(This option is deprecated)"), NULL},
	{NULL}
};


static void
_advance_char (gchar      **in_str,
               gsize       *read_bytes,
               gchar      **out_str,
               gsize       *write_bytes,
               const char  *tmpl)
{
	unsigned char c = (unsigned char) **in_str;
	gchar *repl = g_strdup_printf (tmpl, c);

	(*in_str)++;
	if (read_bytes != NULL)
		(*read_bytes)--;

	*out_str = g_stpcpy (*out_str, (const char *) repl);
	if (write_bytes != NULL)
		*write_bytes -= strlen (repl);

	g_free (repl);
	return;
}

/* Last argument is there to avoid recomputing */
static char *
_filter_cntrl_char (const char *str,
                    const char *tmpl,
                    size_t      byte_per_char)
{
	char *result, *i_ptr, *o_ptr;
	int cnt = 0;

	for (i_ptr = (char *)str; *i_ptr; i_ptr++) {
		if (((*i_ptr > 0) && (*i_ptr < 0x20)) || (*i_ptr == 0x7F)) cnt++;
	}

	if (!cnt)
		return g_strdup (str);

	/* FIXME: any better way to estimate malloc size w/o iterating whole str? */
	result = g_malloc0 (strlen (str) + cnt * byte_per_char);
	o_ptr = result;

	i_ptr = (char *) str;
	while (*i_ptr)
	{
		if (((*i_ptr > 0) && (*i_ptr < 0x20)) || (*i_ptr == 0x7F))
			_advance_char (&i_ptr, NULL, &o_ptr, NULL, tmpl);
		else
			*o_ptr++ = *i_ptr++;
	}

	return result;
}

char *
conv_to_utf8_with_fallback_tmpl (const char *str,
                                 const char *from_enc,
                                 const char *tmpl)
{
	char *u8str, *result, *i_ptr, *o_ptr;
	gsize rbyte, wbyte, status;
	static size_t byte_per_char = 0;
	GIConv conv;

	g_return_val_if_fail ((     str != NULL) && (     *str != '\0'), NULL);
	g_return_val_if_fail ((from_enc != NULL) && (*from_enc != '\0'), NULL);
	g_return_val_if_fail ((    tmpl != NULL) && (    *tmpl != '\0'), NULL);

	/* try the template */
	if (! byte_per_char)
	{
		char *s = g_strdup_printf (tmpl, 0xFF);
		/* UTF-8 character occupies at most 6 bytes */
		byte_per_char = MAX (strlen(s), 6);
		g_free (s);
		/*g_printf ("byte_per_char = %zd\n", byte_per_char);*/
	}
	rbyte = strlen (str);
	wbyte = rbyte * byte_per_char;
	u8str = g_malloc0 (wbyte);

	i_ptr = (char *) str;
	o_ptr = u8str;

	/* Shouldn't fail, as it has been tested upon start of prog */
	conv = g_iconv_open ("UTF-8", from_enc);

	g_debug ("Initial: read=%" G_GSIZE_FORMAT ", write=%" G_GSIZE_FORMAT,
			rbyte, wbyte);

	/* Pass 1: Convert whole string to UTF-8, all illegal seq become escaped hex */
	while (*i_ptr != '\0')
	{
		int e;

		status = g_iconv (conv, &i_ptr, &rbyte, &o_ptr, &wbyte);
		e = errno;

		if ( status != (gsize) -1 ) break;

		g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
			", stt=%" G_GSIZE_FORMAT " (%s) str=%s",
			rbyte, wbyte, status, strerror(e), u8str);

		switch (e) {
			case EILSEQ:
			case EINVAL:
				_advance_char (&i_ptr, &rbyte, &o_ptr, &wbyte, tmpl);
				/* reset state, hopefully Windows don't use stateful encoding at all */
				g_iconv (conv, NULL, NULL, &o_ptr, &wbyte);
                exit_status = R2_ERR_USER_ENCODING;
				break;
			case E2BIG:
				/* Should have already allocated enough buffer. Let it KABOOM! otherwise. */
				g_assert_not_reached();
		}
	}

	g_debug ("r=%02" G_GSIZE_FORMAT ", w=%02" G_GSIZE_FORMAT
		", stt=%" G_GSIZE_FORMAT ", str=%s", rbyte, wbyte, status, u8str);

	g_iconv_close (conv);

	/* Pass 2: Convert all ctrl characters (and some more) to hex */
	/*
	if (! g_utf8_validate (u8str, -1, NULL)) {
		g_critical (_("Intermediate string failed UTF-8 validation"));
		return NULL;
	}
	*/

	result = _filter_cntrl_char (u8str, tmpl, byte_per_char);
	g_free (u8str);

	return result;
}

/*
 * Check if index file has sufficient amount of data for reading
 * 0 = success, all other return status = error
 * If success, infile will be set to file pointer and other args
 * will be filled, otherwise file pointer = NULL
 */
static int
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
			g_printerr (_("For example, if file name was expected to contain "
			              "accented latin characters, use '-l CP1252' option; "
			              "or in case of Japanese Shift JIS characters, '-l CP932'."));
			g_printerr ("\n\n");
#ifdef G_OS_UNIX
			g_printerr (_("Any encoding supported by 'iconv' can be used."));
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
	long         read, write;
	char        *legacy_fname;
	GError      *error = NULL;

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
		record->legacy_filename = conv_to_utf8_with_fallback_tmpl (
			legacy_fname, legacy_encoding, "<\\%02X>");

	g_free (legacy_fname);

	if (! meta.has_unicode_path)
		return record;

	/*******************************************
	 * Part below deals with unicode path only *
	 *******************************************/

	record->uni_filename =
		utf16le_to_utf8 ((gunichar2 *) (buf + UNICODE_FILENAME_OFFSET),
							WIN_PATH_MAX + 1, &read, &write, &error);
	g_debug ("utf16->utf8 read=%li write=%li", read, write);

	if (error)
	{
		g_warning (_("Error converting file name from %s encoding to "
						"UTF-8 encoding for record %u: %s"),
					"UTF-16", record->index_n, error->message);
		g_clear_error (&error);
		exit_status = R2_ERR_INTERNAL;
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

		for (ptr = buf + UNICODE_FILENAME_OFFSET + read * sizeof (gunichar2);
				ptr < buf + UNICODE_RECORD_SIZE; ptr++)
			if ( *(char *) ptr != '\0' )
			{
				g_debug ("Junk detected at offset 0x%tx of unicode path",
					ptr - buf - UNICODE_FILENAME_OFFSET);
				meta.fill_junk = TRUE;
				break;
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

	/* Is charset valid? */
	if (legacy_encoding)
	{
		GIConv try;

		try = g_iconv_open ("UTF-8", legacy_encoding);
		if (try == (GIConv) -1)
		{
			g_printerr (_("'%s' is not a valid encoding on this system. "
				"Only those supported by 'iconv' can be used."),
				legacy_encoding);
			g_printerr ("\n");
#ifdef G_OS_WIN32
			/* TRANSLATOR COMMENT: argument is software name, 'rifiuti2' */
			g_printerr (_("Please visit following web page for a list "
				"closely resembling encodings supported by %s:"), PACKAGE);

			g_printerr ("\n\n\t%s\n", "https://www.gnu.org/software/libiconv/");
#endif
#ifdef G_OS_UNIX
			g_printerr (_("Please execute 'iconv -l' for list of supported encodings."));
			g_printerr ("\n");
#endif
			exit_status = R2_ERR_ARG;
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
	/* Some last minute error messages */
	switch (exit_status) {
		case R2_ERR_USER_ENCODING:
			g_printerr (_("Some entries could not be interpreted in %s encoding, "
				"and characters are displayed in hex value instead. "
				"Very likely the (localised) Windows generating the recycle bin "
				"artifact does not use specified codepage."), legacy_encoding);
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
