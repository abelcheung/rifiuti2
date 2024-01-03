/*
 * Copyright (C) 2007-2023, Abel Cheung
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "utils-error.h"
#include "utils-conv.h"
#include "utils.h"
#include "rifiuti-vista.h"

extern metarecord  *meta;


/**
 * @brief Basic validation of index file
 * @param filename Full path of index file
 * @param filebuf Location of file buffer after reading
 * @param bufsize Location to store size of buffer
 * @param ver Location to store index file version
 * @param error Location to store error upon failure
 * @return `TRUE` if file is deemed usable, `FALSE` otherwise
 * @note This only checks if index file has sufficient amount
 * of data for sensible reading
 */
static bool
_validate_index_file   (const char   *filename,
                        void        **outbuf,
                        gsize        *bufsize,
                        uint64_t     *ver,
                        GError      **error)
{
    char           *buf = NULL;

    g_return_val_if_fail (filename && *filename, false);
    g_return_val_if_fail (outbuf   && ! *outbuf, false);
    g_return_val_if_fail (! error  || ! *error , false);
    g_return_val_if_fail (bufsize  , false);
    g_return_val_if_fail (ver      , false);

    g_debug ("Start file validation for '%s'...", filename);

    if (! g_file_get_contents (filename, &buf, bufsize, error))
        goto validate_fail;

    if (*bufsize <= VERSION1_FILENAME_OFFSET)
    {
        g_set_error_literal (error, R2_REC_ERROR,
        R2_REC_ERROR_IDX_SIZE_INVALID,
            _("File is not a $Recycle.bin index"));
        goto validate_fail;
    }

    copy_field (*ver, buf, VERSION_OFFSET, FILESIZE_OFFSET);
    *ver = GUINT64_FROM_LE (*ver);
    g_debug ("version = %" PRIu64, *ver);

    switch (*ver)
    {
    case VERSION_VISTA: break;  // already handled above

    case VERSION_WIN10:
        // Version 2 adds a uint32 file name strlen before file name.
        // This presumably breaks the 260 char barrier in version 1.
        if (*bufsize <= VERSION2_FILENAME_OFFSET)
        {
            g_set_error_literal (error, R2_REC_ERROR,
            R2_REC_ERROR_IDX_SIZE_INVALID,
                _("File is not a $Recycle.bin index"));
            goto validate_fail;
        }
        break;

    default:
        if (*ver < 10)
            g_set_error (error, R2_REC_ERROR,
                R2_REC_ERROR_VER_UNSUPPORTED,
                _("Index file version %" PRIu64 " is unsupported"), *ver);
        else
            g_set_error (error, R2_REC_ERROR,
                R2_REC_ERROR_VER_UNSUPPORTED,
                "%s", _("File is not a $Recycle.bin index"));
        goto validate_fail;
    }

    *outbuf = buf;
    g_debug ("Finished file validation for '%s'", filename);
    return true;

    validate_fail:
    g_free (buf);
    return false;
}


static rbin_struct *
_populate_record_data  (void      *buf,
                        gsize      bufsize,
                        uint64_t   version)
{
    rbin_struct  *record;
    uint32_t      path_sz_expected, path_sz_actual;
    size_t        null_terminator_offset;
    void         *pathbuf_start = NULL;
    bool          erraneous = false;
    GString      *u;  // shorthand

    switch (version)
    {
    case VERSION_VISTA:
        // In rare cases, the size of index file is one byte short of
        // (fixed) 544 bytes in Vista. Under such occasion, file size
        // only occupies 56 bit, not 64 bit as it ought to be.
        // Actually this 56-bit file size is very likely wrong after all.
        // This is observed during deletion of dd.exe from Forensic
        // Acquisition Utilities (by George M. Garner Jr)
        // in certain localized Vista.
        if (bufsize == VERSION1_FILE_SIZE - 1)
            erraneous = true;

        path_sz_expected = WIN_PATH_MAX * sizeof(gunichar2);
        path_sz_actual = bufsize + (int)erraneous - VERSION1_FILENAME_OFFSET;
        pathbuf_start = buf - (int)erraneous + VERSION1_FILENAME_OFFSET;
        break;

    case VERSION_WIN10:
        copy_field (path_sz_expected, buf, VERSION1_FILENAME_OFFSET,
            VERSION2_FILENAME_OFFSET);
        path_sz_expected = GUINT32_FROM_LE (path_sz_expected) *
            sizeof(gunichar2);
        path_sz_actual = bufsize - VERSION2_FILENAME_OFFSET;
        pathbuf_start = buf + VERSION2_FILENAME_OFFSET;
        break;

    default:
        g_assert_not_reached ();
    }

    record = g_malloc0 (sizeof (rbin_struct));
    record->version = version;

    copy_field (record->filesize, buf, FILESIZE_OFFSET,
        FILETIME_OFFSET - (int) erraneous);
    if (erraneous)
    {
        g_debug ("filesize field broken, 56 bit only, val=0x%" PRIX64,
                 record->filesize);
        /* not printing the value because it was wrong and misleading */
        record->filesize = G_MAXUINT64;
    }
    else
    {
        record->filesize = GUINT64_FROM_LE (record->filesize);
        g_debug ("deleted file size = %" PRIu64, record->filesize);
    }

    /* File deletion time */
    copy_field (record->winfiletime, buf - (int) erraneous,
        FILETIME_OFFSET, VERSION1_FILENAME_OFFSET);
    record->winfiletime = GINT64_FROM_LE (record->winfiletime);
    record->deltime = win_filetime_to_gdatetime (record->winfiletime);

    // Unicode path

    if (path_sz_actual > path_sz_expected)
    {
        g_set_error_literal (&record->error, R2_REC_ERROR,
            R2_REC_ERROR_DUBIOUS_PATH,
            _("Ignored dangling extraneous data after record"));
    }
    else if (path_sz_actual < path_sz_expected && ! erraneous)
    {
        g_set_error_literal (&record->error, R2_REC_ERROR,
            R2_REC_ERROR_DUBIOUS_PATH,
            _("Record is truncated, thus unicode path might be incomplete"));
    }

    u = g_string_new_len ((const char *) pathbuf_start,
        MIN(path_sz_actual, path_sz_expected));
    record->raw_uni_path = u;

    null_terminator_offset = ucs2_bytelen (u->str, u->len);

    if (record->error == NULL)
    {
        char *s = g_convert (u->str, null_terminator_offset,
            "UTF-8", "UTF-16LE", NULL, NULL, NULL);
        if (s)
            g_free (s);
        else
            g_set_error_literal (&record->error, R2_REC_ERROR, R2_REC_ERROR_CONV_PATH,
                _("Path contains broken unicode character(s)"));
    }

    return record;
}

static void
_parse_record_cb   (const char *index_file,
                    metarecord *meta)
{
    rbin_struct       *record = NULL;
    char              *basename = NULL;
    uint64_t           version = 0;
    gsize              bufsize;
    void              *buf = NULL;
    extern bool        isolated_index;
    GError            *error = NULL;

    basename = g_path_get_basename (index_file);

    if (! _validate_index_file (index_file,
        &buf, &bufsize, &version, &error))
    {
        g_hash_table_replace (meta->invalid_records,
            g_strdup (basename), error);
        g_free (basename);
        return;
    }

    g_debug ("Start populating record for '%s'...", basename);

    record = _populate_record_data (buf, bufsize, version);
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
        g_debug ("Bad entry %s, meta ver = %" PRId64
            ", rec ver = %" PRId64,
            record->index_s, meta->version, (int64_t)record->version);
        meta->version = VERSION_INCONSISTENT;
    }
}

/**
 * @brief Determine overall version from all `$Recycle.bin` index files
 * @param meta The metadata for recycle bin
 * @return `FALSE` if multiple versions are found, otherwise `TRUE`
 */
static bool
_set_overall_rbin_version (metarecord *meta)
{
    if (! meta->records->len) {
        meta->version = VERSION_NOT_FOUND;
        return true;
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

    if (! rifiuti_init (
        RECYCLE_BIN_TYPE_DIR,
        N_("DIR_OR_FILE"),
        N_("Parse index files in C:\\$Recycle.bin style "
           "folder and dump recycle bin data.  "
           "Can also dump a single index file."),
        &argv, &error
    ))
        goto cleanup;

    do_parse_records (&_parse_record_cb);

    if (! meta->records->len && g_hash_table_size (meta->invalid_records))
    {
        g_set_error_literal (&error, R2_FATAL_ERROR,
            R2_FATAL_ERROR_ILLEGAL_DATA,
            _("No valid recycle bin record found"));
        goto cleanup;
    }

    g_ptr_array_sort (meta->records, _sort_record_by_time);
    if (! _set_overall_rbin_version (meta))
    {
        g_set_error_literal (&error, R2_FATAL_ERROR,
            R2_FATAL_ERROR_ILLEGAL_DATA,
            _("Index files from multiple Windows versions are mixed together."
            "  Please check each file individually."));
        goto cleanup;
    }

    if (! dump_content (&error))
    {
        g_assert (error->domain == G_FILE_ERROR);
        GError *new_err = g_error_new_literal (
            R2_FATAL_ERROR, R2_FATAL_ERROR_TEMPFILE,
            g_strdup (error->message));
        g_error_free (error);
        error = new_err;
    }

    cleanup:

    return rifiuti_cleanup (&error);
}
