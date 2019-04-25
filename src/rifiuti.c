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


static r2status     exit_status          = EXIT_SUCCESS;
static metarecord   meta;
extern char        *legacy_encoding;

/* 0-25 => A-Z, 26 => '\', 27 or above is erraneous */
unsigned char   driveletters[28] =
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U',
	'V', 'W', 'X', 'Y', 'Z', '\\', '?'
};

/*!
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
			g_printerr ("\n");

			status = R2_ERR_ARG;
			goto validation_broken;
		}

		switch (ver)
		{
			case VERSION_WIN95: meta.os_guess = OS_GUESS_95; break;
			case VERSION_WIN98: meta.os_guess = OS_GUESS_98; break;
			case VERSION_ME_03: meta.os_guess = OS_GUESS_ME; break;
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
	rbin_struct    *record;
	uint64_t        win_filetime;
	uint32_t        drivenum;
	size_t          read;
	char           *legacy_fname;

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
		record->legacy_path = conv_path_to_utf8_with_tmpl (
			legacy_fname, legacy_encoding, "<\\%02X>", &read, &exit_status);

		if (record->legacy_path == NULL) {
			g_warning (_("(Record %u) Error converting legacy path to UTF-8."),
				record->index_n);
			record->legacy_path = "";
		}
	}

	g_free (legacy_fname);

	if (! meta.has_unicode_path)
		return record;

	/*******************************************
	 * Part below deals with unicode path only *
	 *******************************************/

	record->uni_path = conv_path_to_utf8_with_tmpl (
		(char *) (buf + UNICODE_FILENAME_OFFSET), NULL,
		"<\\u%04X>", &read, &exit_status);

	if (record->uni_path == NULL) {
		g_warning (_("(Record %u) Error converting unicode path to UTF-8."),
			record->index_n);
		record->uni_path = "";
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

int
main (int    argc,
      char **argv)
{
	GSList             *filelist   = NULL;
	GSList             *recordlist = NULL;
	GOptionContext     *context;

	extern char       **fileargs;

	rifiuti_init (argv[0]);

	/* TRANSLATOR: appears in help text short summary */
	context = g_option_context_new (N_("INFO2"));
	g_option_context_set_summary (context, N_(
		"Parse INFO2 file and dump recycle bin data."));
	rifiuti_setup_opt_ctx (&context, RECYCLE_BIN_TYPE_FILE);
	exit_status = rifiuti_parse_opt_ctx (&context, &argc, &argv);
	if (exit_status != EXIT_SUCCESS)
		goto cleanup;

	if (!fileargs || g_strv_length (fileargs) > 1)
	{
		g_printerr (_("Must specify exactly one INFO2 file as argument."));
		g_printerr ("\n");
		g_printerr (_("Run program without any option for more info."));
		g_printerr ("\n");
		exit_status = R2_ERR_ARG;
		goto cleanup;
	}

	exit_status = check_file_args (fileargs[0], &filelist, RECYCLE_BIN_TYPE_FILE);
	if (exit_status != EXIT_SUCCESS)
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

	/* Print everything */
	{
		r2status s = prepare_output_handle ();
		if (s != EXIT_SUCCESS) {
			exit_status = s;
			goto cleanup;
		}
	}

	print_header (meta);
	g_slist_foreach (recordlist, (GFunc) print_record_cb, NULL);
	print_footer ();

	close_output_handle ();

	/* file descriptor should have been closed at this point */
	{
		r2status s = move_temp_file ();
		if ( s != EXIT_SUCCESS )
			exit_status = s;
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
	free_vars ();

	return exit_status;
}
