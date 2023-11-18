/*
 * Copyright (C) 2007-2023, Abel Cheung
 * This package is released under Revised BSD License.
 * Please see docs/LICENSE.txt for more info.
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

static r2status     exit_status          = EXIT_SUCCESS;
static metarecord   meta;


/*!
 * Check if index file has sufficient amount of data for reading.
 * If successful, its file content will be stored in buf.
 * Return 0 if successful, non-zero on error
 */
static r2status
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
                           R2_ERR_INTERNAL );
    g_return_val_if_fail ( (filebuf  != NULL), R2_ERR_INTERNAL );
    g_return_val_if_fail ( (bufsize  != NULL), R2_ERR_INTERNAL );
    g_return_val_if_fail ( (ver      != NULL), R2_ERR_INTERNAL );
    g_return_val_if_fail ( (pathlen  != NULL), R2_ERR_INTERNAL );

    if ( !g_file_get_contents (filename, &buf, bufsize, &err) )
    {
        g_critical (_("%s(): failed to retrieve file content for '%s': %s"),
            __func__, filename, err->message);
        g_clear_error (&err);
        status = R2_ERR_OPEN_FILE;
        goto validation_error;
    }

    g_debug ("Retrieval of '%s' is done, size = %" G_GSIZE_FORMAT, filename, *bufsize);

    if (*bufsize <= VERSION1_FILENAME_OFFSET)
    {
        g_debug ("File size expected to be more than %" G_GSIZE_FORMAT,
            (gsize) VERSION1_FILENAME_OFFSET);
        g_printerr ("%s", _("File is truncated, or probably not a $Recycle.bin index file."));
        g_printerr ("\n");
        status = R2_ERR_BROKEN_FILE;
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
                g_debug ("File size expected to be %" G_GSIZE_FORMAT
                    " or %" G_GSIZE_FORMAT, expected, expected - 1);
                g_printerr ("%s", _("Index file expected size and real size do not match."));
                g_printerr ("\n");
                status = R2_ERR_BROKEN_FILE;
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
                g_printerr ("%s", _("Index file expected size and real size do not match."));
                g_printerr ("\n");
                status = R2_ERR_BROKEN_FILE;
                goto validation_error;
            }
            break;

        default:
            g_printerr ("%s", _("Unsupported file version, or probably not a $Recycle.bin index file."));
            g_printerr ("\n");
            status = R2_ERR_BROKEN_FILE;
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
    rbin_struct  *record;
    size_t        read;

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
    memcpy (&record->winfiletime, buf + FILETIME_OFFSET - (int) erraneous,
            VERSION1_FILENAME_OFFSET - FILETIME_OFFSET);
    record->winfiletime = GINT64_FROM_LE (record->winfiletime);
    record->deltime = win_filetime_to_gdatetime (record->winfiletime);

    /* One extra char for safety, though path should have already been null terminated */
    g_debug ("pathlen = %d", pathlen);
    switch (version)
    {
        case VERSION_VISTA:
            record->uni_path = conv_path_to_utf8_with_tmpl (
                (const char *) (buf - erraneous + VERSION1_FILENAME_OFFSET),
                NULL, "<\\u%04X>", &read, &exit_status);
            break;

        case VERSION_WIN10:
            record->uni_path = conv_path_to_utf8_with_tmpl (
                (const char *) (buf + VERSION2_FILENAME_OFFSET),
                NULL, "<\\u%04X>", &read, &exit_status);
            break;

        default:
            g_assert_not_reached ();
    }

    if (record->uni_path == NULL) {
        g_warning (_("(Record %s) Error converting unicode path to UTF-8."),
            record->index_s);
        record->uni_path = "";
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
    r2status        validate_st;

    basename = g_path_get_basename (index_file);

    validate_st = validate_index_file (
        index_file, &buf, &bufsize, &version, &pathlen);
    if ( validate_st != EXIT_SUCCESS )
    {
        g_printerr (_("File '%s' fails validation."), basename);
        g_printerr ("\n");
        exit_status = validate_st;
        goto parse_record_error;
    }

    g_debug ("Start populating record for '%s'...", basename);

    switch (version)
    {
        case VERSION_VISTA:
            /* see populate_record_data() for meaning of last parameter */
            record = populate_record_data (buf, version, pathlen, (bufsize == VERSION1_FILE_SIZE - 1));
            break;

        case VERSION_WIN10:
            record = populate_record_data (buf, version, pathlen, FALSE);
            break;

        default:
            g_assert_not_reached();
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
    return ((a->winfiletime < b->winfiletime) ? -1 :
            (a->winfiletime > b->winfiletime) ?  1 :
            strcmp (a->index_s, b->index_s));
}


int
main (int    argc,
      char **argv)
{
    GSList             *filelist   = NULL;
    GSList             *recordlist = NULL;
    GOptionContext     *context;

    extern char       **fileargs;
    extern gboolean     live_mode;

    rifiuti_init (argv[0]);

    /* TRANSLATOR: appears in help text short summary */
    context = g_option_context_new (N_("DIR_OR_FILE"));
    g_option_context_set_summary (context, N_(
        "Parse index files in C:\\$Recycle.bin style folder "
        "and dump recycle bin data.  Can also dump a single index file."));
    rifiuti_setup_opt_ctx (&context, RECYCLE_BIN_TYPE_DIR);
    exit_status = rifiuti_parse_opt_ctx (&context, &argc, &argv);
    if (exit_status != EXIT_SUCCESS)
        goto cleanup;

#ifdef G_OS_WIN32
    extern gboolean live_mode;

    if (live_mode) {
        GSList *bindirs = enumerate_drive_bins();
        GSList *ptr = bindirs;
        while (ptr) {
            check_file_args (ptr->data, &filelist, RECYCLE_BIN_TYPE_DIR);
            ptr = ptr->next;
        }
        ptr = NULL;
        g_slist_free_full (bindirs, g_free);
    }
    else
#endif
    {
        exit_status = check_file_args (fileargs[0], &filelist, RECYCLE_BIN_TYPE_DIR);
        if (exit_status != EXIT_SUCCESS)
            goto cleanup;
    }

    g_slist_foreach (filelist, (GFunc) parse_record_cb, &recordlist);

    /* Fill in recycle bin metadata */
    meta.type               = RECYCLE_BIN_TYPE_DIR;
    meta.keep_deleted_entry = FALSE;
    meta.is_empty           = (filelist == NULL);
    meta.has_unicode_path   = TRUE;
    if (live_mode)
        meta.filename = "(current system)";
    else
        meta.filename = fileargs[0];

    /* NULL filelist at this point means a valid empty $Recycle.bin */
    if ( !meta.is_empty && (recordlist == NULL) )
    {
        g_printerr ("%s", _("No valid recycle bin index file found."));
        g_printerr ("\n");
        exit_status = R2_ERR_BROKEN_FILE;
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

            while (NULL != (l = l->next))
            {
                if ((int64_t) ((rbin_struct *) l->data)->version != meta.version)
                {
                    meta.version = VERSION_INCONSISTENT;
                    break;
                }
            }

            if (meta.version == VERSION_INCONSISTENT)
            {
                g_printerr ("%s", _("Index files come from multiple versions of Windows."
                    "  Please check each file independently."));
                g_printerr ("\n");
                exit_status = R2_ERR_BROKEN_FILE;
                goto cleanup;
            }
        }
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
            g_printerr ("%s", _("Some entries could not be presented as correct "
                "unicode path.  The concerned characters are displayed "
                "in escaped unicode sequences."));
            g_printerr ("\n");
            break;

        default:
            break;
    }

    g_debug ("Cleaning up...");

    g_slist_free_full (recordlist, (GDestroyNotify) free_record_cb);
    g_slist_free_full (filelist  , (GDestroyNotify) g_free        );
    free_vars ();

    close_error_handle ();

    return exit_status;
}
