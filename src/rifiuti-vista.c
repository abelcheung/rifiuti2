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

#include <errno.h>
#include <stdlib.h>

#include "utils.h"
#ifdef G_OS_WIN32
#  include "utils-win.h"
#endif

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "rifiuti-vista.h"

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

static GOptionEntry mainoptions[] =
{
	{"output", 'o', 0, G_OPTION_ARG_FILENAME, &outfilename,
	 N_("Write output to FILE"), N_("FILE")},
	{"xml", 'x', 0, G_OPTION_ARG_NONE, &xml_output,
	 N_("Output in XML format instead of tab-delimited values"), NULL},
	{"localtime", 'z', 0, G_OPTION_ARG_NONE, &use_localtime,
	 N_("Present deletion time in time zone of local system (default is UTC)"),
	 NULL},
	{"version", 'v', 0, G_OPTION_ARG_NONE, &do_print_version,
	 N_("Print version information and exit"), NULL},
	{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
	 N_("$Recycle.bin folder or file name"), NULL},
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
 * Check if index file has sufficient amount of data for reading.
 * If successful, its file content will be stored in buf.
 * Return 0 if successful, non-zero on error
 */
static int
validate_index_file (const char  *filename,
                     void       **filebuf,
                     gsize       *bufsize,
                     uint64_t    *ver,
                     uint32_t    *pathlen)
{
	gsize           expected;
	int             status;
	GError         *err = NULL;
	char           *buf;

	g_debug ("Start file validation for '%s'...", filename);

	g_return_val_if_fail ( (filename != NULL) && (*filename != '\0'),
	                       RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (filebuf  != NULL), RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (bufsize  != NULL), RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (ver      != NULL), RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (pathlen  != NULL), RIFIUTI_ERR_INTERNAL );

	if ( !g_file_get_contents (filename, &buf, bufsize, &err) )
	{
		g_critical (_("%s(): failed to retrieve file content for '%s': %s"),
		            __func__, filename, err->message);
		g_clear_error (&err);
		status = RIFIUTI_ERR_OPEN_FILE;
		goto validation_error;
	}

	g_debug ("Retrieval of '%s' is done, size = %" G_GSIZE_FORMAT, filename, *bufsize);

	if (*bufsize <= VERSION1_FILENAME_OFFSET)
	{
		g_debug ("File size expected to be more than %" G_GSIZE_FORMAT,
		         (gsize) VERSION1_FILENAME_OFFSET);
		g_printerr (_("File is truncated, or probably not a $Recycle.bin index file.\n"));
		status = RIFIUTI_ERR_BROKEN_FILE;
		goto validation_error;
	}

	copy_field (ver, VERSION, FILESIZE);
	*ver = GUINT64_FROM_LE (*ver);
	g_debug ("version = %" G_GUINT64_FORMAT, *ver);

	switch (*ver)
	{
	  case VERSION_VISTA:

		expected = VERSION1_FILE_SIZE;
		/* see populate_record_data() for reason */
		if ( (*bufsize != expected) && (*bufsize != expected - 1) )
		{
			g_debug ("File size expected to be %" G_GSIZE_FORMAT " or %" G_GSIZE_FORMAT,
			         expected, expected - 1);
			g_printerr (_("Index file expected size and real size do not match.\n"));
			status = RIFIUTI_ERR_BROKEN_FILE;
			goto validation_error;
		}
		*pathlen = WIN_PATH_MAX;
		break;

	  case VERSION_WIN10:

		copy_field (pathlen, VERSION1_FILENAME, VERSION2_FILENAME);
		*pathlen = GUINT32_FROM_LE (*pathlen);

		/* Header length + file name length in UTF-16 encoding */
		expected = VERSION2_FILENAME_OFFSET + (*pathlen) * 2;
		if (*bufsize != expected)
		{
			g_debug ("File size expected to be %" G_GSIZE_FORMAT, expected);
			g_printerr (_("Index file expected size and real size do not match.\n"));
			status = RIFIUTI_ERR_BROKEN_FILE;
			goto validation_error;
		}
		break;

	  default:
		g_printerr (_("File is not supported, or it is probably "
		              "not a $Recycle.bin index file.\n"));
		status = RIFIUTI_ERR_BROKEN_FILE;
		goto validation_error;
	}

	*filebuf = buf;
	return EXIT_SUCCESS;

  validation_error:
	*filebuf = NULL;
	return status;
}


static rbin_struct *
populate_record_data (void *buf,
                      uint64_t version,
                      uint32_t pathlen,
                      gboolean erraneous)
{
	uint64_t        win_filetime;
	rbin_struct    *record;
	GError         *error = NULL;
	long            read, write;

	record = g_malloc0 (sizeof (rbin_struct));
	record->version = version;

	/*
	 * In rare cases, the size of index file is 543 bytes versus (normal) 544 bytes.
	 * In such occasion file size only occupies 56 bit, not 64 bit as it ought to be.
	 * Actually this 56-bit file size is very likely wrong after all. Probably some
	 * bug inside Windows. This is observed during deletion of dd.exe from Forensic
	 * Acquisition Utilities (by George M. Garner Jr) in certain localized Vista.
	 */
	memcpy (&record->filesize, buf + FILESIZE_OFFSET,
			FILETIME_OFFSET - FILESIZE_OFFSET - (int) erraneous);
	if (erraneous)
	{
		g_debug ("filesize field broken, 56 bit only, val=0x%" G_GINT64_MODIFIER "X",
		         record->filesize);
		/* not printing the value because it was wrong and misleading */
		record->filesize = G_MAXUINT64;
	}
	else
	{
		record->filesize = GUINT64_FROM_LE (record->filesize);
		g_debug ("deleted file size = %" G_GUINT64_FORMAT, record->filesize);
	}

	/* File deletion time */
	memcpy (&win_filetime, buf + FILETIME_OFFSET - (int) erraneous,
	        VERSION1_FILENAME_OFFSET - FILETIME_OFFSET);
	win_filetime = GUINT64_FROM_LE (win_filetime);
	record->deltime = win_filetime_to_epoch (win_filetime);

	/* One extra char for safety, though path should have already been null terminated */
	g_debug ("pathlen = %d", pathlen);
	switch (version)
	{
	  case VERSION_VISTA:
		record->utf8_filename =
			utf16le_to_utf8 ((gunichar2 *) (buf + VERSION1_FILENAME_OFFSET - (int) erraneous),
			                 pathlen + 1, &read, &write, &error);
		break;
	  case VERSION_WIN10:
		record->utf8_filename =
			utf16le_to_utf8 ((gunichar2 *) (buf + VERSION2_FILENAME_OFFSET),
			                 pathlen + 1, &read, &write, &error);
		break;
	}
	g_debug ("utf16->8 r=%li w=%li", read, write);

	if (error)
	{
		g_warning (_("Error converting file name from %s encoding to "
		             "UTF-8 encoding: %s"), "UTF-16", error->message);
		g_clear_error (&error);
	}
	return record;
}

static void
parse_record_cb (char    *index_file,
                 GSList **recordlist)
{
	rbin_struct    *record;
	char           *basename;
	uint64_t        version = 0;
	uint32_t        pathlen = 0;
	gsize           bufsize;
	void           *buf = NULL;

	basename = g_path_get_basename (index_file);

	exit_status = validate_index_file (index_file,
	                                   &buf, &bufsize, &version, &pathlen);
	if ( exit_status != EXIT_SUCCESS )
	{
		g_printerr (_("File '%s' fails validation.\n"), basename);
		goto parse_record_error;
	}

	g_debug ("Start populating record for '%s'...", basename);

	switch (version)
	{
	  case VERSION_VISTA:
		/* see populate_record_data() for meaning of last parameter */
		record = populate_record_data (buf, version, pathlen,
		                               (bufsize == VERSION1_FILE_SIZE - 1));
		break;

	  case VERSION_WIN10:
		record = populate_record_data (buf, version, pathlen, FALSE);
		break;

	  default:
		/* VERY wrong if reaching here. Version info has already been filtered once */
		g_critical (_("Version info for '%s' still wrong "
		              "despite file validation."), basename);
		goto parse_record_error;
	}

	g_debug ("Parsing done for '%s'", basename);
	record->index_s = basename;
	record->meta = &meta;
	*recordlist = g_slist_prepend (*recordlist, record);
	g_free (buf);
	return;

  parse_record_error:
	g_free (buf);
	g_free (basename);
}


static int
sort_record_by_time (rbin_struct *a,
                     rbin_struct *b)
{
	/* sort primary key: deletion time; secondary key: index file name */
	return ((a->deltime < b->deltime) ? -1 :
	        (a->deltime > b->deltime) ?  1 :
	        strcmp (a->index_s, b->index_s));
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

	context = g_option_context_new (_("DIR_OR_FILE"));
	g_option_context_set_summary
		(context, _("Parse index files in C:\\$Recycle.bin style folder "
		            "and dump recycle bin data.  Can also dump "
		            "a single index file."));
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
		g_printerr (_("Must specify exactly one directory containing "
		              "$Recycle.bin index files, or one such index file, "
		              "as argument.\n\n"));
		g_printerr (_("Run program with '-?' option for more info.\n"));
		exit_status = RIFIUTI_ERR_ARG;
		goto cleanup;
	}

	if (xml_output)
	{
		output_format = OUTPUT_XML;
		if (no_heading || always_utf8 || (NULL != delim))
		{
			g_printerr (_("Plain text format options can not "
			              "be used in XML mode.\n"));
			exit_status = RIFIUTI_ERR_ARG;
			goto cleanup;
		}
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

	exit_status = check_file_args (fileargs[0], &filelist, FALSE);
	if ( EXIT_SUCCESS != exit_status )
		goto cleanup;

	g_slist_foreach (filelist, (GFunc) parse_record_cb, &recordlist);

	/* Fill in recycle bin metadata */
	meta.type = RECYCLE_BIN_TYPE_DIR;
	meta.filename = fileargs[0];
	meta.keep_deleted_entry = FALSE;
	meta.os_guess = NULL;  /* TOOD */
	meta.is_empty = (filelist == NULL);
	meta.has_unicode_path = TRUE;

	/* NULL filelist at this point means a valid empty $Recycle.bin */
	if ( !meta.is_empty && (recordlist == NULL) )
	{
		g_printerr ("%s", _("No valid recycle bin index file found.\n"));
		exit_status = RIFIUTI_ERR_BROKEN_FILE;
		goto cleanup;
	}
	recordlist = g_slist_sort (recordlist, (GCompareFunc) sort_record_by_time);

	/* detect global recycle bin version from versions of all files */
	{
		GSList  *l = recordlist;
		if (!l)
			meta.version = VERSION_NOT_FOUND;
		else
		{
			meta.version = (int64_t) ((rbin_struct *) recordlist->data)->version;
			while ( (NULL != (l = l->next)) && (meta.version != VERSION_INCONSISTENT) )
				if ((int64_t) ((rbin_struct *) l->data)->version != meta.version)
				{
					meta.version = VERSION_INCONSISTENT;
					exit_status = RIFIUTI_ERR_BROKEN_FILE;
				}
		}
	}

	if (outfilename)
	{
		int tmpfile;
		tmppath = g_strdup ("rifiuti-XXXXXX");
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

	/*
	 * TODO: Store errors accumulated when parsing each file, then print a summary
	 * of errors after normal result, instead of dumping all errors on the spot
	 */
	if (!no_heading)
		print_header (outfile, meta);
	g_slist_foreach (recordlist, (GFunc) print_record_cb, outfile);
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
	g_slist_foreach (recordlist, (GFunc) free_record_cb, NULL);
	g_slist_free (recordlist);

	g_slist_foreach (filelist, (GFunc) g_free, NULL);
	g_slist_free (filelist);

	g_strfreev (fileargs);
	g_free (outfilename);
	g_free (delim);

	return exit_status;
}

/* vim: set sw=4 ts=4 noexpandtab : */
