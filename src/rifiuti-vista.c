/*
 * Copyright (C) 2007-2023, Abel Cheung
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "rifiuti-vista.h"
#include "utils.h"
#ifdef G_OS_WIN32
#  include "utils-win.h"
#endif

static r2status     exit_status = R2_OK;
extern GSList      *filelist;
extern metarecord  *meta;

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
    GError         *error = NULL;
    char           *buf;

    g_debug ("Start file validation for '%s'...", filename);

    g_return_val_if_fail ( (filename != NULL) && (*filename != '\0'),
                           R2_ERR_INTERNAL );
    g_return_val_if_fail ( (filebuf  != NULL), R2_ERR_INTERNAL );
    g_return_val_if_fail ( (bufsize  != NULL), R2_ERR_INTERNAL );
    g_return_val_if_fail ( (ver      != NULL), R2_ERR_INTERNAL );
    g_return_val_if_fail ( (pathlen  != NULL), R2_ERR_INTERNAL );

    if ( !g_file_get_contents (filename, &buf, bufsize, &error) )
    {
        g_critical (_("%s(): failed to retrieve file content for '%s': %s"),
            __func__, filename, error->message);
        g_clear_error (&error);
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

    copy_field (ver, VERSION_OFFSET, FILESIZE_OFFSET);
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

            copy_field (pathlen, VERSION1_FILENAME_OFFSET, VERSION2_FILENAME_OFFSET);
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
    return R2_OK;

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
parse_record_cb (char *index_file,
                 const metarecord *meta)
{
    rbin_struct    *record = NULL;
    char           *basename = NULL;
    uint64_t        version = 0;
    uint32_t        pathlen = 0;
    gsize           bufsize;
    void           *buf = NULL;
    extern gboolean isolated_index;


    basename = g_path_get_basename (index_file);

    {
        r2status sts = validate_index_file (
            index_file, &buf, &bufsize, &version, &pathlen);
        if ( sts != R2_OK )
        {
            g_printerr (_("File '%s' fails validation.\n"), basename);
            exit_status = sts;
            g_free (buf);
            g_free (basename);
            return;
        }
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
            g_assert_not_reached();
    }

    g_free (buf);

    /* Check corresponding $R.... file existance and set record->gone */
    if (isolated_index)
        record->gone = FILESTATUS_UNKNOWN;
    else
    {
        char *dirname = g_path_get_dirname (index_file);
        char *trash_basename = g_strdup (basename);
        trash_basename[1] = 'R';  /* $R... versus $I... */
        char *trash_path = g_build_filename (dirname, trash_basename, NULL);
        record->gone = g_file_test (trash_path, G_FILE_TEST_EXISTS) ?
            FILESTATUS_EXISTS : FILESTATUS_GONE;
        g_free (dirname);
        g_free (trash_basename);
        g_free (trash_path);
    }

    record->index_s = basename;
    g_ptr_array_add (meta->records, record);

    g_debug ("Parsing done for '%s'", basename);
}


static int
_sort_record_by_time (gconstpointer left,
                      gconstpointer right)
{
    const rbin_struct *a = *((rbin_struct **) left);
    const rbin_struct *b = *((rbin_struct **) right);

    /* sort by deletion time, then index file name */
    return ((a->winfiletime < b->winfiletime) ? -1 :
            (a->winfiletime > b->winfiletime) ?  1 :
            strcmp (a->index_s, b->index_s));
}

static void
_compare_idx_versions (rbin_struct *record,
                       metarecord *meta)
{
    if (meta->version == VERSION_INCONSISTENT)
        return;

    if (meta->version != (int64_t) record->version) {
        g_critical ("Bad entry at %s, meta ver = %" G_GINT64_FORMAT ", rec = %" G_GINT64_FORMAT,
            record->index_s, meta->version, (int64_t)record->version);
        meta->version = VERSION_INCONSISTENT;
    }
}

/**
 * @brief Determine overall version from all `$Recycle.bin` index files
 * @param meta The metadata for recycle bin
 * @return `FALSE` if multiple versions are found, otherwise `TRUE`
 */
static gboolean
_set_overall_rbin_version (metarecord *meta)
{
    if (! meta->records->len) {
        meta->version = VERSION_NOT_FOUND;
        return TRUE;
    }

    meta->version = ((rbin_struct *)(meta->records->pdata[0]))->version;
    g_ptr_array_foreach (meta->records, (GFunc) _compare_idx_versions, meta);

    return (meta->version != VERSION_INCONSISTENT);
}

int
main (int    argc,
      char **argv)
{
    GError *error = NULL;

    UNUSED (argc);

    exit_status = rifiuti_init (
        RECYCLE_BIN_TYPE_DIR,
        N_("DIR_OR_FILE"),
        N_("Parse index files in C:\\$Recycle.bin style "
           "folder and dump recycle bin data.  "
           "Can also dump a single index file."),
        &argv, &error
    );
    if (exit_status != R2_OK)
        goto cleanup;

    g_slist_foreach (filelist, (GFunc) parse_record_cb, meta);

    if (! meta->records->len && (exit_status != R2_OK))
    {
        g_printerr ("%s", _("No valid recycle bin record found.\n"));
        exit_status = R2_ERR_BROKEN_FILE;
        goto cleanup;
    }

    g_ptr_array_sort (meta->records, _sort_record_by_time);
    if (! _set_overall_rbin_version (meta))
    {
        g_printerr ("%s", _("Index files come from multiple versions of Windows."
            "  Please check each file independently."));
        g_printerr ("\n");
        exit_status = R2_ERR_BROKEN_FILE;
        goto cleanup;
    }

    if (!dump_content (&error))
        exit_status = R2_ERR_WRITE_FILE;

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
            if (error) {
                g_printerr ("%s\n", error->message);
            }
            break;
    }

    rifiuti_cleanup ();
    g_clear_error (&error);

    return exit_status;
}
