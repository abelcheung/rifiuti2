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
       gboolean   has_unicode_filename = TRUE;
       gboolean   use_localtime        = FALSE;
static gboolean   do_print_version     = FALSE;

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


/* Check if index file has sufficient amount of data for reading */
/* 0 = success, all other return status = error */
static int
validate_index_file (const char  *filename,
                     FILE       **infile,
                     off_t       *size,
                     uint64_t    *ver,
                     uint32_t    *pathlen)
{
	off_t           expected;
	size_t          status;
	GStatBuf        st;
	FILE           *fp = NULL;

	g_debug ("Start file validation...");

	g_return_val_if_fail ( (infile  != NULL), RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (size    != NULL), RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (ver     != NULL), RIFIUTI_ERR_INTERNAL );
	g_return_val_if_fail ( (pathlen != NULL), RIFIUTI_ERR_INTERNAL );
	*infile = NULL;

	if (0 != g_stat (filename, &st))
	{
		g_printerr (_("Error getting metadata of file '%s': %s\n"), filename,
		            strerror (errno));
		return RIFIUTI_ERR_OPEN_FILE;
	}

	if (st.st_size <= VERSION1_FILENAME_OFFSET)	/* file path can't possibly be empty */
	{
		g_debug ("file size = %d, expect at least %d\n", (int) st.st_size,
		         VERSION1_FILENAME_OFFSET);
		g_printerr (_("File is truncated, or probably not a $Recycle.bin index file.\n"));
		return RIFIUTI_ERR_BROKEN_FILE;
	}
	*size = st.st_size;

	if ( !(fp = g_fopen (filename, "rb")) )
	{
		g_printerr (_("Error opening file '%s' for reading: %s\n"), filename,
		            strerror (errno));
		return RIFIUTI_ERR_OPEN_FILE;
	}

	/* with file size check already done, fread fail -> serious problem */
	status = fread (ver, sizeof (*ver), 1, fp);
	if (status < 1)
	{
		/* TRANSLATOR COMMENT: the variable is function name */
		g_critical (_("%s(): fread() failed when reading version info from '%s'"),
		            __func__, filename);
		return RIFIUTI_ERR_OPEN_FILE;
	}
	*ver = GUINT64_FROM_LE (*ver);
	g_debug ("version=%" G_GUINT64_FORMAT, *ver);

	switch (*ver)
	{
	  case (uint64_t) VERSION_VISTA:

		expected = VERSION1_FILE_SIZE;
		/* see populate_record_data() for reason */
		if ( (*size != expected) && (*size != expected - 1) )
		{
			g_debug ("File size = %" G_GUINT64_FORMAT ", expected %" G_GUINT64_FORMAT,
					 (uint64_t) *size, (uint64_t) expected);
			g_printerr (_("Index file expected size and real size do not match.\n"));
			status = RIFIUTI_ERR_BROKEN_FILE;
			goto validation_broken;
		}
		break;

	  case (uint64_t) VERSION_WIN10:

		fseek (fp, VERSION2_FILENAME_OFFSET - sizeof (*pathlen),
		       SEEK_SET);
		status = fread (pathlen, sizeof (*pathlen), 1, fp);
		if (status < 1)
		{
			/* TRANSLATOR COMMENT: the variable is function name */
			g_critical (_("%s(): fread() failed when reading file name length"),
			            __func__);
			status = RIFIUTI_ERR_OPEN_FILE;
			goto validation_broken;
		}
		*pathlen = GUINT32_FROM_LE (*pathlen);

		/* Fixed header length + file name length in UTF-16 encoding */
		expected = VERSION2_FILENAME_OFFSET + (*pathlen) * 2;
		if (*size != expected)
		{
			g_debug ("File size = %" G_GUINT64_FORMAT ", expected %" G_GUINT64_FORMAT,
			         (uint64_t) *size, (uint64_t) expected);
			g_printerr (_("Index file expected size and real size do not match.\n"));
			status = RIFIUTI_ERR_BROKEN_FILE;
			goto validation_broken;
		}
		break;

	  default:
		g_printerr (_("File is not supported, or it is probably "
		              "not a $Recycle.bin index file.\n"));
		status = RIFIUTI_ERR_BROKEN_FILE;
		goto validation_broken;
	}

	rewind (fp);
	*infile = fp;
	return EXIT_SUCCESS;

  validation_broken:
	fclose (fp);
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
	/* TODO: Consider if the (possibly wrong) size should be printed or not */
	memcpy (&record->filesize, buf + FILESIZE_OFFSET,
	        FILETIME_OFFSET - FILESIZE_OFFSET - (int) erraneous);
	record->filesize = GUINT64_FROM_LE (record->filesize);
	g_debug ("filesize=%" G_GUINT64_FORMAT, record->filesize);

	/* File deletion time */
	memcpy (&win_filetime, buf + FILETIME_OFFSET - (int) erraneous,
	        VERSION1_FILENAME_OFFSET - FILETIME_OFFSET);
	win_filetime = GUINT64_FROM_LE (win_filetime);
	record->deltime = win_filetime_to_epoch (win_filetime);

	/* One extra char for safety, though path should have already been null terminated */
	g_debug ("pathlen=%d", pathlen);
	switch (version)
	{
	  case (uint64_t) VERSION_VISTA:
		record->utf8_filename =
			utf16le_to_utf8 ((gunichar2 *) (buf + VERSION1_FILENAME_OFFSET - (int) erraneous),
			                 pathlen + 1, &read, &write, &error);
		break;
	  case (uint64_t) VERSION_WIN10:
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
parse_record (char    *index_file,
              GSList **recordlist)
{
	FILE           *infile;
	rbin_struct    *record;
	char           *basename;
	uint64_t        version;
	uint32_t        pathlen;
	off_t           bufsize;
	void           *buf;

	basename = g_path_get_basename (index_file);

	if (EXIT_SUCCESS != validate_index_file (index_file,
				&infile, &bufsize, &version, &pathlen))
	{
		g_printerr (_("File '%s' fails validation.\n"), basename);
		goto parse_record_open_error;
	}

	/* Files are expected to be at most 0.5KB. Large files should have already been rejected. */
	buf = g_malloc0 (bufsize + 2);
	if (1 != fread (buf, bufsize, 1, infile))
	{
		/*
		 * TRANSLATOR COMMENT: 1st parameter is function name,
		 * 2nd is file name, 3rd is error message
		 */
		g_critical (_("%s(): fread() failed when "
		              "reading content of file '%s': %s\n"),
		            __func__, basename, strerror (errno));
		goto parse_validation_error;
	}

	g_debug ("Start populating record for '%s'...", basename);

	switch (version)
	{
	  case VERSION_VISTA:
		/* see populate_record_data() for meaning of last parameter */
		record = populate_record_data (buf, version, (uint32_t) WIN_PATH_MAX,
		                               (bufsize == VERSION1_FILE_SIZE - 1));
		break;

	  case VERSION_WIN10:
		record = populate_record_data (buf, version, pathlen, FALSE);
		break;

	  default:
		/* VERY wrong if reaching here. Version info has already been filtered once */
		g_critical (_("Version info for '%s' still wrong "
		              "despite file validation."), basename);
		goto parse_validation_error;
	}

	g_debug ("Parsing done for '%s'", basename);
	record->index_s = basename;
	*recordlist = g_slist_prepend (*recordlist, record);
	fclose (infile);
	g_free (buf);
	return;

  parse_validation_error:
	fclose (infile);

  parse_record_open_error:
	g_free (basename);
}


/* Scan folder and add all "$Ixxxxxx.xxx" to filelist for parsing */
static void
populate_index_file_list (GSList **list,
                          char    *path)
{
	GDir           *dir;
	char           *direntry, *fname;
	GPatternSpec   *pattern1, *pattern2;
	GError         *error = NULL;

	if (NULL == (dir = g_dir_open (path, 0, &error)))
	{
		g_printerr (_("Error opening directory '%s': %s\n"), path,
		            error->message);
		g_clear_error (&error);
		exit (RIFIUTI_ERR_OPEN_FILE);
	}

	pattern1 = g_pattern_spec_new ("$I??????.*");
	pattern2 = g_pattern_spec_new ("$I??????");

	while ((direntry = (char *) g_dir_read_name (dir)) != NULL)
	{
		if (!g_pattern_match_string (pattern1, direntry) &&
		    !g_pattern_match_string (pattern2, direntry))
			continue;
		fname = g_build_filename (path, direntry, NULL);
		*list = g_slist_prepend (*list, fname);
	}

	g_dir_close (dir);

	g_pattern_spec_free (pattern1);
	g_pattern_spec_free (pattern2);
}

/* Search for desktop.ini in folder for hint of recycle bin */
static gboolean
found_desktop_ini (char *path)
{
	char *filename, *content, *found;

	filename = g_build_filename (path, "desktop.ini", NULL);
	if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
		goto desktop_ini_error;

	/* assume desktop.ini is ASCII and not something spurious */
	if (!g_file_get_contents (filename, &content, NULL, NULL))
		goto desktop_ini_error;

	/* Don't bother parsing, we don't use the content at all */
	found = strstr (content, RECYCLE_BIN_CLSID);
	g_free (content);
	g_free (filename);
	return (found != NULL);

  desktop_ini_error:
	g_free (filename);
	return FALSE;
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
	metarecord      meta;
	char           *fname;
	GOptionContext *context;

	rifiuti_init (argv[0]);

	context = g_option_context_new (_("DIR_OR_FILE"));
	g_option_context_set_summary
		(context, _("Parse index files in C:\\$Recycle.bin style folder "
		            "and dump recycle bin data.  Can also dump "
		            "a single index file."));
	rifiuti_setup_opt_ctx (&context, mainoptions, textoptions);
	rifiuti_parse_opt_ctx (&context, &argc, &argv);

	if (do_print_version)
		print_version(); /* bye bye */

	if (!fileargs || g_strv_length (fileargs) > 1)
	{
		g_printerr (_("Must specify exactly one directory containing "
		              "$Recycle.bin index files, or one such index file, "
		              "as argument.\n\n"));
		g_printerr (_("Run program with '-?' option for more info.\n"));
		exit (RIFIUTI_ERR_ARG);
	}

	if (xml_output)
	{
		output_format = OUTPUT_XML;
		if (no_heading || always_utf8 || (NULL != delim))
		{
			g_printerr (_("Plain text format options can not "
			              "be used in XML mode.\n"));
			exit (RIFIUTI_ERR_ARG);
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

	g_debug ("Start basic file checking...");

	if (outfilename)
	{
		outfile = g_fopen (outfilename, "wb");
		if (NULL == outfile)
		{
			g_printerr (_("Error opening file '%s' for writing: %s\n"),
			            outfilename, strerror (errno));
			exit (RIFIUTI_ERR_OPEN_FILE);
		}
	}
	else
		outfile = stdout;

	if (!g_file_test (fileargs[0], G_FILE_TEST_EXISTS))
	{
		g_printerr (_("'%s' does not exist.\n"), fileargs[0]);
		exit (RIFIUTI_ERR_OPEN_FILE);
	}
	else if (g_file_test (fileargs[0], G_FILE_TEST_IS_DIR))
	{
		populate_index_file_list (&filelist, fileargs[0]);
		if (NULL == filelist)
		{
			/* last ditch effort: search for desktop.ini. Just print empty content
			 * representing empty recycle bin if found.
			 */
			if (!found_desktop_ini (fileargs[0]))
			{
				g_printerr (_("No files with name pattern '%s' are found in "
				              "directory. Probably not a $Recycle.bin directory.\n"),
				            "$Ixxxxxx.*");
				exit (RIFIUTI_ERR_OPEN_FILE);
			}
		}
	}
	else if (g_file_test (fileargs[0], G_FILE_TEST_IS_REGULAR))
	{
		fname = g_strdup (fileargs[0]);
		filelist = g_slist_prepend (filelist, fname);
	}
	else
	{
		g_printerr (_("'%s' is not a normal file or directory.\n"),
		            fileargs[0]);
		exit (RIFIUTI_ERR_OPEN_FILE);
	}

	g_slist_foreach (filelist, (GFunc) parse_record, &recordlist);

	/* NULL filelist at this point means a valid empty $Recycle.bin */
	if ((filelist != NULL) && (recordlist == NULL))
	{
		g_printerr ("%s", _("No valid recycle bin index file found.\n"));
		g_slist_foreach (filelist, (GFunc) g_free, NULL);
		g_slist_free (filelist);
		exit (RIFIUTI_ERR_BROKEN_FILE);
	}
	recordlist = g_slist_sort (recordlist, (GCompareFunc) sort_record_by_time);

	/* Fill in recycle bin metadata */
	meta.type = RECYCLE_BIN_TYPE_DIR;
	meta.keep_deleted_entry = FALSE;
	meta.filename = fileargs[0];
	{
		GSList  *l = recordlist;
		if (!l)
			meta.version = VERSION_NOT_FOUND;
		else
		{
			meta.version = (int64_t) ((rbin_struct *) recordlist->data)->version;
			for (; l != NULL; l = l->next)
			{
				if ((int64_t) ((rbin_struct *) l->data)->version != meta.version)
					meta.version = VERSION_INCONSISTENT;
				((rbin_struct *) l->data)->meta = &meta;
			}
		}
	}
	meta.os_guess = NULL;  /* TOOD */

	if (!no_heading)
		print_header (outfile, meta);

	/* TODO: store return status of each file, then exit the program with last non-zero status */
	/* TODO: store errors accumulated when parsing each file, then print a summary of errors
	 * after normal result, instead of dumping all errors on the spot */
	g_slist_foreach (recordlist, (GFunc) print_record, outfile);

	print_footer (outfile);

	g_debug ("Cleaning up...");

	/* g_slist_free_full() available only since 2.28 */
	g_slist_foreach (recordlist, (GFunc) free_record, NULL);
	g_slist_free (recordlist);

	g_slist_foreach (filelist, (GFunc) g_free, NULL);
	g_slist_free (filelist);

	fclose (outfile);

	g_strfreev (fileargs);
	g_free (outfilename);
	g_free (delim);

	exit (EXIT_SUCCESS);
}

/* vim: set sw=4 ts=4 noexpandtab : */
