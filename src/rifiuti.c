/*
 * Copyright (C) 2003, Keith J. Jones.
 * Copyright (C) 2007-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "rifiuti.h"
#include "utils.h"


static exitcode     exit_status = R2_OK;
extern char        *legacy_encoding;
extern GSList      *filelist;
extern metarecord  *meta;


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
static gboolean
_validate_index_file   (const char   *filename,
                        FILE        **infile,
                        GError      **error)
{
    void           *buf;
    FILE           *fp = NULL;
    uint32_t        ver;
    int             e;

    g_return_val_if_fail (filename && *filename, FALSE);
    g_return_val_if_fail (infile && ! *infile, FALSE);

    g_debug ("Start file validation for '%s'...", filename);

    if (! (fp = g_fopen (filename, "rb")))
    {
        e = errno;
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno(e),
            _("Can not open file: %s"), g_strerror(e));
        return FALSE;
    }

    /* empty recycle bin = 20 bytes */
    buf = g_malloc (RECORD_START_OFFSET);
    if (1 > fread (buf, RECORD_START_OFFSET, 1, fp))
    {
        g_set_error_literal (error, R2_FATAL_ERROR,
            R2_FATAL_ERROR_ILLEGAL_DATA,
            _("File is prematurely truncated, or not an INFO2 index."));
        goto validation_broken;
    }

    copy_field (&ver, VERSION_OFFSET, KEPT_ENTRY_OFFSET);
    ver = GUINT32_FROM_LE (ver);

    // total_entry only meaningful for 95 and NT4, on other versions
    // it's junk memory data, don't bother copying
    if ( ( ver == VERSION_NT4 ) || ( ver == VERSION_WIN95 ) ) {
        copy_field (&meta->total_entry, TOTAL_ENTRY_OFFSET, RECORD_SIZE_OFFSET);
        meta->total_entry = GUINT32_FROM_LE (meta->total_entry);
    }

    copy_field (&meta->recordsize, RECORD_SIZE_OFFSET, FILESIZE_SUM_OFFSET);
    meta->recordsize = GUINT32_FROM_LE (meta->recordsize);

    g_free (buf);

    switch (meta->recordsize)
    {
        case LEGACY_RECORD_SIZE:

            if (( ver != VERSION_ME_03 ) &&  /* ME -> 280 byte record */
                ( ver != VERSION_WIN98 ) &&
                ( ver != VERSION_WIN95 ))
            {
                g_set_error (error, R2_FATAL_ERROR, R2_FATAL_ERROR_ILLEGAL_DATA,
                    "Illegal INFO2 version %" PRIu32, ver);
                goto validation_broken;
            }

            if (!legacy_encoding)
            {
                g_set_error_literal (error, G_OPTION_ERROR,
                    G_OPTION_ERROR_FAILED,
                    "This INFO2 file was produced on a legacy system "
                    "without Unicode file name (Windows ME or earlier). "
                    "Please specify codepage of concerned system with "
                    "'-l' option.");
                goto validation_broken;
            }
            break;

        case UNICODE_RECORD_SIZE:

            if (ver != VERSION_ME_03 && ver != VERSION_NT4)
            {
                g_set_error (error, R2_FATAL_ERROR, R2_FATAL_ERROR_ILLEGAL_DATA,
                    "Illegal INFO2 version %" PRIu32, ver);
                goto validation_broken;
            }
            break;

        default:
            g_set_error (error, R2_FATAL_ERROR, R2_FATAL_ERROR_ILLEGAL_DATA,
                _("Illegal INFO2 of record size %" G_GSIZE_FORMAT),
                meta->recordsize);
            goto validation_broken;
    }

    rewind (fp);
    *infile = fp;
    meta->version = (uint64_t) ver;
    return TRUE;

    validation_broken:

    fclose (fp);
    return FALSE;
}


static rbin_struct *
_populate_record_data   (void     *buf,
                         gsize     bufsize,
                         gboolean *junk_detected)
{
    rbin_struct    *record;
    uint32_t        drivenum;
    size_t          read;
    char           *legacy_fname;

    record = g_malloc0 (sizeof (rbin_struct));

    legacy_fname = g_malloc0 (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);
    copy_field (legacy_fname, LEGACY_FILENAME_OFFSET, RECORD_INDEX_OFFSET);

    /* Index number associated with the record */
    copy_field (&record->index_n, RECORD_INDEX_OFFSET, DRIVE_LETTER_OFFSET);
    record->index_n = GUINT32_FROM_LE (record->index_n);
    g_debug ("index=%u", record->index_n);

    /* Number representing drive letter, 'A:' = 0, etc */
    copy_field (&drivenum, DRIVE_LETTER_OFFSET, FILETIME_OFFSET);
    drivenum = GUINT32_FROM_LE (drivenum);
    g_debug ("drive=%u", drivenum);
    if (drivenum >= sizeof (driveletters) - 1) {
        g_set_error (&record->error, R2_REC_ERROR,
            R2_REC_ERROR_DRIVE_LETTER,
            _("Drive number %" PRIu32 "does not represent "
            "a valid drive"), drivenum);
    }
    record->drive = driveletters[MIN (drivenum, sizeof (driveletters) - 1)];

    record->gone = FILESTATUS_EXISTS;
    // If file is not in recycle bin (restored or permanently deleted),
    // first byte will be removed from filename
    if (!*legacy_fname)
    {
        record->gone = FILESTATUS_GONE;
        *legacy_fname = record->drive;
    }

    /* File deletion time */
    copy_field (&record->winfiletime, FILETIME_OFFSET, FILESIZE_OFFSET);
    record->winfiletime = GINT64_FROM_LE (record->winfiletime);
    record->deltime = win_filetime_to_gdatetime (record->winfiletime);

    /* File size or occupied cluster size */
    /* BEWARE! This is 32bit data casted to 64bit struct member */
    copy_field (&record->filesize, FILESIZE_OFFSET, UNICODE_FILENAME_OFFSET);
    record->filesize = GUINT64_FROM_LE (record->filesize);
    g_debug ("filesize=%" PRIu64, record->filesize);

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
            legacy_fname, legacy_encoding,
            "<\\%02X>", &read, &record->error);
    }

    g_free (legacy_fname);

    if (bufsize == LEGACY_RECORD_SIZE)
        return record;

    /* Part below deals with unicode path only */

    record->uni_path = conv_path_to_utf8_with_tmpl (
        (char *) (buf + UNICODE_FILENAME_OFFSET), NULL,
        "<\\u%04X>", &read, &record->error);

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
    if (junk_detected && ! *junk_detected)
    {
        void *ptr;

        for (ptr = buf + UNICODE_FILENAME_OFFSET + read;
            ptr < buf + UNICODE_RECORD_SIZE; ptr++)
        {
            if ( *(char *) ptr != '\0' )
            {
                g_debug ("Junk detected at offset 0x%tx of unicode path",
                    ptr - buf - UNICODE_FILENAME_OFFSET);
                *junk_detected = TRUE;
                break;
            }
        }
    }

    return record;
}


static void
_parse_record_cb   (char *index_file,
                    metarecord *meta)
{
    rbin_struct   *record;
    FILE          *infile = NULL;
    gsize          read_sz, record_sz;
    void          *buf = NULL;
    GError        *error = NULL;
    int64_t        prev_pos, curr_pos;

    if (! _validate_index_file (index_file, &infile, &error))
    {
        g_hash_table_replace (meta->invalid_records,
            g_strdup (index_file), error);
        return;
    }

    g_debug ("Start populating record for '%s'...", index_file);

    record_sz = meta->recordsize;
    buf = g_malloc0 (record_sz);

    fseek (infile, RECORD_START_OFFSET, SEEK_SET);
    curr_pos = (int64_t) ftell (infile);
    prev_pos = curr_pos;

    while ((read_sz = fread (buf, 1, record_sz, infile)) > 0)
    {
        prev_pos = curr_pos;
        curr_pos = (int64_t) ftell (infile);
        g_debug ("Read %s, byte range %" PRId64 " - %" PRId64,
            index_file, prev_pos, curr_pos);
        if (read_sz < record_sz) {
            g_debug ("read size = %zu, less than needed %zu", read_sz, record_sz);
            break;
        }
        record = _populate_record_data (buf, record_sz, &meta->fill_junk);
        g_ptr_array_add (meta->records, record);
    }
    g_free (buf);

    char *segment_id = g_strdup_printf ("|%" PRId64 "|%" PRId64, prev_pos, curr_pos);

    if (feof (infile) && read_sz && (read_sz < record_sz))
    {
        g_set_error_literal (&error, R2_REC_ERROR,
            R2_REC_ERROR_IDX_SIZE_INVALID,
            _("Last segment does not constitute a valid "
            "record. Likely a premature end of file."));
    }
    else if (ferror (infile))  // other generic error
    {
        g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
            _("Failed to read record at %s"), segment_id);
    }

    if (error) {
        g_hash_table_replace (meta->invalid_records,
            g_strdup (segment_id), error);
    }
    g_free (segment_id);
    fclose (infile);
}

int
main (int    argc,
      char **argv)
{
    GError *error = NULL;

    UNUSED (argc);

    if (! rifiuti_init (
        RECYCLE_BIN_TYPE_FILE,
        N_("INFO2"),
        N_("Parse INFO2 file and dump recycle bin data."),
        &argv, &error
    ))
        goto cleanup;

    g_slist_foreach (filelist, (GFunc) _parse_record_cb, meta);

    if (! meta->records->len && g_hash_table_size (meta->invalid_records))
    {
        g_set_error_literal (&error, R2_FATAL_ERROR,
            R2_FATAL_ERROR_ILLEGAL_DATA,
            _("No valid recycle bin record found"));
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

    exit_status = rifiuti_handle_global_error (error);
    if (rifiuti_handle_record_error () && exit_status == R2_OK)
        exit_status = R2_ERR_DUBIOUS_DATA;

    rifiuti_cleanup ();
    return exit_status;
}
