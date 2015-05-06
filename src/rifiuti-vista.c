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
	 N_("Output in XML format instead of tab-delimiated values "
	    "(plain text options disallowed in this case)"), NULL},
	{"localtime", 'z', 0, G_OPTION_ARG_NONE, &use_localtime,
	 N_("Present deletion time in time zone of local system (default is UTC)"),
	 NULL},
	{"version", 'v', 0, G_OPTION_ARG_NONE, &do_print_version,
	 N_("Print version information and exit"), NULL},
	{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &fileargs,
	 N_("File names"), NULL},
	{NULL}
};

static GOptionEntry textoptions[] =
{
	{"delimiter", 't', 0, G_OPTION_ARG_STRING, &delim,
	 N_("String to use as delimiter (TAB by default)"), N_("STRING")},
	{"no-heading", 'n', 0, G_OPTION_ARG_NONE, &no_heading,
	 N_("Don't show header info"), NULL},
	{"always-utf8", '8', 0, G_OPTION_ARG_NONE, &always_utf8,
	 N_("Always show file names in UTF-8 encoding"), NULL},
	{NULL}
};


/* Check if index file has sufficient amount of data for reading */
/* 0 = success, all other return status = error */
static int
validate_index_file (FILE *inf,
                     off_t size,
                     uint64_t *version,
                     uint32_t *namelength)
{
	off_t    expected;
	size_t   status;

	g_debug ("Start file validation...");

	if (size <= VERSION1_FILENAME_OFFSET)	/* file path can't possibly be empty */
	{
		g_debug ("file size = %i, expect larger than %i\n", (int) size,
		         VERSION1_FILENAME_OFFSET);
		g_critical (_("File is truncated, or probably "
		              "not a $Recycle.bin index file."));
		return RIFIUTI_ERR_BROKEN_FILE;
	}

	/* with file size check already done, fread fail probably mean serious problem */
	rewind (inf);
	status = fread (version, sizeof (*version), 1, inf);
	if (status < 1)
	{
		/* TRANSLATOR COMMENT: the variable is function name */
		g_critical (_("%s(): fread() failed when reading version info"),
		            __func__);
		return RIFIUTI_ERR_OPEN_FILE;
	}
	*version = GUINT64_FROM_LE (*version);
	g_debug ("version=%" G_GUINT64_FORMAT, *version);

	switch (*version)
	{
	  case (uint64_t) FORMAT_VISTA:
		expected = VERSION1_FILE_SIZE;
		/* see populate_record_data() for reason */
		if ((size == expected) || (size == expected - 1))
			return 0;
		break;

	  case (uint64_t) FORMAT_WIN10:
		g_return_val_if_fail ((size > VERSION2_FILENAME_OFFSET), FALSE);
		fseek (inf, VERSION2_FILENAME_OFFSET - sizeof (*namelength),
		       SEEK_SET);
		if (status < 1)
		{
			/* TRANSLATOR COMMENT: the variable is function name */
			g_critical (_("%s(): fread() failed when reading file name length"),
			            __func__);
			return RIFIUTI_ERR_OPEN_FILE;
		}
		status = fread (namelength, sizeof (*namelength), 1, inf);
		*namelength = GUINT32_FROM_LE (*namelength);

		/* Fixed header length + file name length in UTF-16 encoding */
		expected = VERSION2_FILENAME_OFFSET + (*namelength) * 2;
		if (size == expected)
			return 0;
		break;

	  default:
		g_printerr (_("File is not supported, or it is probably "
		              "not a $Recycle.bin index file.\n"));
		return RIFIUTI_ERR_BROKEN_FILE;
	}

	g_debug ("File size = %" G_GUINT64_FORMAT ", expected %" G_GUINT64_FORMAT,
	         (uint64_t) size, (uint64_t) expected);
	g_printerr (_("Index file expected size and real size do not match.\n"));
	return RIFIUTI_ERR_BROKEN_FILE;
}


static rbin_struct *
populate_record_data (void *buf,
                      uint64_t version,
                      uint32_t namelength,
                      gboolean erraneous)
{
	uint64_t        win_filetime;
	rbin_struct    *record;
	gunichar2      *utf16_filename;
	GError         *error = NULL;
	long            read, write;

	record = g_malloc0 (sizeof (rbin_struct));
	record->version = version;
	record->type = RECYCLE_BIN_TYPE_DIR;

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
	g_debug ("filesize=%" G_GUINT64_FORMAT, record->filesize);

	/* File deletion time */
	memcpy (&win_filetime, buf + FILETIME_OFFSET - (int) erraneous,
	        VERSION1_FILENAME_OFFSET - FILETIME_OFFSET);
	win_filetime = GUINT64_FROM_LE (win_filetime);
	record->deltime = win_filetime_to_epoch (win_filetime);

	/* One extra char for safety, though path should have already been null terminated */
	utf16_filename = g_malloc0 (2 * (namelength + 1));
	g_debug ("namelength=%d", namelength);
	switch (version)
	{
	  case (uint64_t) FORMAT_VISTA:
		memcpy (utf16_filename,
		        buf + VERSION1_FILENAME_OFFSET - (int) erraneous,
		        namelength * 2);
		break;
	  case (uint64_t) FORMAT_WIN10:
		memcpy (utf16_filename, buf + VERSION2_FILENAME_OFFSET,
		        namelength * 2);
		break;
	}
	record->utf8_filename =
		g_utf16_to_utf8 (utf16_filename, -1, &read, &write, &error);
	g_debug ("utf16->8 r=%li w=%li", read, write);

	if (error)
	{
		g_warning (_("Error converting file name from %s encoding to "
		             "UTF-8 encoding: %s"), "UTF-16", error->message);
		g_clear_error (&error);
	}

	g_free (utf16_filename);
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
	uint32_t        namelength = 0;
	GStatBuf        st;
	void           *buf;

	basename = g_path_get_basename (index_file);

	if (0 != g_stat (index_file, &st))
	{
		g_printerr (_("Error getting metadata of file '%s': %s\n"), basename,
		            strerror (errno));
		goto parse_record_open_error;
	}

	if (NULL == (infile = g_fopen (index_file, "rb")))
	{
		g_printerr (_("Error opening file '%s' for reading: %s\n"), basename,
		            strerror (errno));
		goto parse_record_open_error;
	}

	if (0 != validate_index_file (infile, st.st_size, &version, &namelength))
	{
		g_printerr (_("File '%s' fails validation.\n"), basename);
		goto parse_validation_error;
	}

	rewind (infile);
	/* Files are expected to be at most 0.5KB. Large files should have already been rejected. */
	buf = g_malloc0 (st.st_size + 2);
	if (1 != fread (buf, st.st_size, 1, infile))
	{
		g_critical (_("%s(): fread() '%s' failed when "
		              "reading file content: %s\n"),
		            __func__, basename, strerror (errno));
		goto parse_validation_error;
	}

	g_debug ("Start populating record for '%s'...", basename);

	switch (version)
	{
	  case (uint64_t) FORMAT_VISTA:
		/* see populate_record_data() for meaning of last parameter */
		record = populate_record_data (buf, version, (uint32_t) WIN_PATH_MAX,
		                               (st.st_size == VERSION1_FILE_SIZE - 1));
		break;

	  case (uint64_t) FORMAT_WIN10:
		record = populate_record_data (buf, version, namelength, FALSE);
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


static void
free_record (rbin_struct *record)
{
	g_free (record->index_s);
	g_free (record->utf8_filename);
	g_free (record);
}


static int
sort_record_by_time (rbin_struct *a,
                     rbin_struct *b)
{
	/* time_t can be 32 or 64 bit, can't just return a-b :( */
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
	char           *fname, *bug_report_str;

	GError         *error = NULL;
	GOptionContext *context;
	GOptionGroup   *textoptgroup;

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

	context = g_option_context_new (_("DIR_OR_FILE"));
	g_option_context_set_summary
		(context, _("Parse index files in C:\\$Recycle.bin style folder "
		            "and dump recycle bin data.  Can also dump "
		            "a single index file."));
	bug_report_str =
		g_strdup_printf (_("Report bugs to %s"), PACKAGE_BUGREPORT);
	g_option_context_set_description (context, bug_report_str);
	g_free (bug_report_str);
	g_option_context_add_main_entries (context, mainoptions, "rifiuti");

	textoptgroup =
		g_option_group_new ("text", _("Plain text output options:"),
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

	{
		gboolean i;

		/* The user case where this code won't provide benefit is VERY rare,
		 * so don't bother doing fallback because it never worked for them
		 */
#if GLIB_CHECK_VERSION(2, 40, 0) && defined (G_OS_WIN32)
		char **args;

		args = g_win32_get_command_line ();
		i = g_option_context_parse_strv (context, &args, &error);
		g_strfreev (args);
#else
		i = g_option_context_parse (context, &argc, &argv, &error);
#endif
		g_option_context_free (context);

		if (!i)
		{
			g_printerr (_("Error parsing options: %s\n"), error->message);
			g_clear_error (&error);
			exit (RIFIUTI_ERR_ARG);
		}
	}

	if (do_print_version)
	{
		print_version();
		exit (EXIT_SUCCESS);
	}

	if (!fileargs || g_strv_length (fileargs) > 1)
	{
		g_printerr (_("Must specify exactly one directory containing "
		              "$Recycle.bin index files, or one such index file, "
		              "as argument.\n\n"));
		g_printerr (_("Run program with '-?' option for more info.\n"));
		exit (RIFIUTI_ERR_ARG);
	}

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

	g_debug ("Start basic file checking...");

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
				              "directory.\nProbably not a $Recycle.bin directory.\n"),
				            "$Ixxxxxx.xxx");
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

	{
		GSList  *l = recordlist;
		int64_t  ver;

		if (!l)
			ver = VERSION_NOT_FOUND;
		else
		{
			ver = (int64_t) ((rbin_struct *) recordlist->data)->version;
			for (; l != NULL; l = l->next)
				if ((int64_t) ((rbin_struct *) l->data)->version != ver)
				{
					ver = VERSION_INCONSISTENT;
					break;
				}
		}
		if (!no_heading)
			print_header (outfile, fileargs[0], ver, FALSE);
	}

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
