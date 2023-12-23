/*
 * Copyright (C) 2003, Keith J. Jones.
 * Copyright (C) 2007-2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "utils-error.h"
#include "utils-conv.h"
#include "utils.h"
#include "rifiuti.h"


extern char        *legacy_encoding;
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
    void           *buf = NULL;
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
        goto validation_fail;
    }

    copy_field (ver, buf, VERSION_OFFSET, KEPT_ENTRY_OFFSET);
    ver = GUINT32_FROM_LE (ver);

    // total_entry only meaningful for 95 and NT4, on other versions
    // it's junk memory data, don't bother copying
    if ( ( ver == VERSION_NT4 ) || ( ver == VERSION_WIN95 ) ) {
        copy_field (meta->total_entry, buf, TOTAL_ENTRY_OFFSET, RECORD_SIZE_OFFSET);
        meta->total_entry = GUINT32_FROM_LE (meta->total_entry);
    }

    copy_field (meta->recordsize, buf, RECORD_SIZE_OFFSET, FILESIZE_SUM_OFFSET);
    meta->recordsize = GUINT32_FROM_LE (meta->recordsize);

    g_free (buf);
    buf = NULL;

    switch (meta->recordsize)
    {
        case LEGACY_RECORD_SIZE:

            if (( ver != VERSION_ME_03 ) &&  /* ME -> 280 byte record */
                ( ver != VERSION_WIN98 ) &&
                ( ver != VERSION_WIN95 ))
            {
                g_set_error (error, R2_FATAL_ERROR, R2_FATAL_ERROR_ILLEGAL_DATA,
                    "Illegal INFO2 version %" PRIu32, ver);
                goto validation_fail;
            }

            if (!legacy_encoding)
            {
                g_set_error_literal (error, G_OPTION_ERROR,
                    G_OPTION_ERROR_FAILED,
                    "This INFO2 file was produced on a legacy system "
                    "without Unicode file name (Windows ME or earlier). "
                    "Please specify codepage of concerned system with "
                    "'-l' option.");
                goto validation_fail;
            }
            break;

        case UNICODE_RECORD_SIZE:

            if (ver != VERSION_ME_03 && ver != VERSION_NT4)
            {
                g_set_error (error, R2_FATAL_ERROR, R2_FATAL_ERROR_ILLEGAL_DATA,
                    "Illegal INFO2 version %" PRIu32, ver);
                goto validation_fail;
            }
            break;

        default:
            g_set_error (error, R2_FATAL_ERROR, R2_FATAL_ERROR_ILLEGAL_DATA,
                _("Illegal INFO2 of record size %" G_GSIZE_FORMAT),
                meta->recordsize);
            goto validation_fail;
    }

    rewind (fp);
    *infile = fp;
    meta->version = ver;
    return TRUE;

    validation_fail:

    g_free (buf);
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
    size_t          uni_buf_sz, null_terminator_offset;

    record = g_malloc0 (sizeof (rbin_struct));

    // Verbatim path in ANSI code page
    record->raw_legacy_path = g_malloc0 (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET);
    copy_field (*(record->raw_legacy_path), buf,
        LEGACY_FILENAME_OFFSET, RECORD_INDEX_OFFSET);

    /* Index number associated with the record */
    copy_field (record->index_n, buf, RECORD_INDEX_OFFSET, DRIVE_LETTER_OFFSET);
    record->index_n = GUINT32_FROM_LE (record->index_n);
    g_debug ("index=%u", record->index_n);

    /* Number representing drive letter, 'A:' = 0, etc */
    copy_field (drivenum, buf, DRIVE_LETTER_OFFSET, FILETIME_OFFSET);
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
    if (! *record->raw_legacy_path)
    {
        record->gone = FILESTATUS_GONE;
        *record->raw_legacy_path = record->drive;
    }

    /* File deletion time */
    copy_field (record->winfiletime, buf, FILETIME_OFFSET, FILESIZE_OFFSET);
    record->winfiletime = GINT64_FROM_LE (record->winfiletime);
    record->deltime = win_filetime_to_gdatetime (record->winfiletime);

    /* File size or occupied cluster size */
    /* BEWARE! This is 32bit data casted to 64bit struct member */
    copy_field (record->filesize, buf,
        FILESIZE_OFFSET, UNICODE_FILENAME_OFFSET);
    record->filesize = GUINT64_FROM_LE (record->filesize);
    g_debug ("filesize=%" PRIu64, record->filesize);

    // Only bother checking legacy path when requested,
    // because otherwise we don't know which encoding to use
    if (legacy_encoding)
    {
        char *s = g_convert (record->raw_legacy_path, -1,
            "UTF-8", legacy_encoding, NULL, NULL, NULL);
        if (s)
            g_free (s);
        else
            g_set_error (&record->error, R2_REC_ERROR, R2_REC_ERROR_CONV_PATH,
                _("Path contains character(s) that could not be "
                "interpreted in %s encoding"), legacy_encoding);
    }

    if (bufsize == LEGACY_RECORD_SIZE)
        return record;

    /* Part below deals with unicode path only */

    uni_buf_sz = UNICODE_RECORD_SIZE - UNICODE_FILENAME_OFFSET;
    record->raw_uni_path = g_malloc (uni_buf_sz);
    copy_field (*(record->raw_uni_path), buf,
        UNICODE_FILENAME_OFFSET, UNICODE_RECORD_SIZE);
    null_terminator_offset = ucs2_strnlen (
        record->raw_uni_path, WIN_PATH_MAX) * sizeof (gunichar2);

    {
        // Never set len = -1 for wchar source string
        char *s = g_convert (record->raw_uni_path, null_terminator_offset,
            "UTF-8", "UTF-16LE", NULL, NULL, NULL);
        if (s)
        {
            g_free (s);
        }
        else
        {
            g_set_error_literal (&record->error, R2_REC_ERROR, R2_REC_ERROR_CONV_PATH,
                _("Path contains broken unicode character(s)"));
        }
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
     *
     * The 8.3 path generated from non-ascii seems to follow certain
     * ruleset, but the exact detail is unknown:
     * - accented latin chars transliterated to pure ASCII
     * - first DBCS char converted to UCS2 codepoint
     */
    if (junk_detected && ! *junk_detected)
    {
        // Beware: start pos shouldn't be previously read bytes,
        // as it may contain invalid seq and quit prematurely.
        char *p = record->raw_uni_path + null_terminator_offset;

        while (p < record->raw_uni_path + uni_buf_sz)
        {
            if (*p != '\0')
            {
                g_debug ("Junk detected at offset 0x%tx of unicode path",
                    p - record->raw_uni_path);
                *junk_detected = TRUE;
                break;
            }
            p++;
        }

        if (*junk_detected)
            hexdump (record->raw_uni_path, uni_buf_sz);
    }

    return record;
}


static void
_parse_record_cb   (const char *index_file,
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

    do_parse_records (&_parse_record_cb);

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

    return rifiuti_cleanup (&error);
}
