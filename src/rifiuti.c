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


static r2status     exit_status = R2_OK;
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
static r2status
validate_index_file (const char  *filename,
                     FILE       **infile,
                     metarecord  *meta)
{
    void           *buf;
    FILE           *fp = NULL;
    uint32_t        ver;
    int             e, ret;

    g_debug ("Start file validation...");

    g_return_val_if_fail ( infile != NULL, R2_ERR_INTERNAL );
    *infile = NULL;

    if ( !(fp = g_fopen (filename, "rb")) )
    {
        e = errno;
        g_printerr (_("Error opening file '%s' for reading: %s"),
            filename, g_strerror (e));
        g_printerr ("\n");
        return R2_ERR_OPEN_FILE;
    }

    buf = g_malloc (RECORD_START_OFFSET);

    if ( 1 > fread (buf, RECORD_START_OFFSET, 1, fp) )
    {
        /* TRANSLATOR COMMENT: file size must be at least 20 bytes */
        g_critical (_("File size less than minimum allowed (%d bytes)"), RECORD_START_OFFSET);
        ret = R2_ERR_BROKEN_FILE;
        goto validation_broken;
    }

    copy_field (&ver, VERSION_OFFSET, KEPT_ENTRY_OFFSET);
    ver = GUINT32_FROM_LE (ver);

    /* total_entry only meaningful for 95 and NT4, on other versions
     * it's junk memory data, don't bother copying */
    if ( ( ver == VERSION_NT4 ) || ( ver == VERSION_WIN95 ) ) {
        copy_field (&meta->total_entry, TOTAL_ENTRY_OFFSET, RECORD_SIZE_OFFSET);
        meta->total_entry = GUINT32_FROM_LE (meta->total_entry);
    }

    copy_field (&meta->recordsize, RECORD_SIZE_OFFSET, FILESIZE_SUM_OFFSET);
    meta->recordsize = GUINT32_FROM_LE (meta->recordsize);

    g_free (buf);

    /* Turns out version is not reliable indicator. Use size instead */
    switch (meta->recordsize)
    {
      case LEGACY_RECORD_SIZE:

        if ( ( ver != VERSION_ME_03 ) &&  /* ME still use 280 byte record */
             ( ver != VERSION_WIN98 ) &&
             ( ver != VERSION_WIN95 ) )
        {
            g_printerr ("%s", _("Unsupported file version, or probably not an INFO2 file at all."));
            g_printerr ("\n");
            ret = R2_ERR_BROKEN_FILE;
            goto validation_broken;
        }

        if (!legacy_encoding)
        {
            g_printerr ("%s", _("This INFO2 file was produced on a legacy system "
                          "without Unicode file name (Windows ME or earlier). "
                          "Please specify codepage of concerned system with "
                          "'-l' or '--legacy-filename' option."));
            g_printerr ("\n\n");
            /* TRANSLATOR COMMENT: can choose example from YOUR language & code page */
            g_printerr ("%s", _("For example, if recycle bin is expected to come from West "
                          "European versions of Windows, use '-l CP1252' option; "
                          "or in case of Japanese Windows, use '-l CP932'."));
            g_printerr ("\n");

            ret = R2_ERR_ARG;
            goto validation_broken;
        }

        break;

      case UNICODE_RECORD_SIZE:

        if ( ( ver != VERSION_ME_03 ) && ( ver != VERSION_NT4 ) )
        {
            g_printerr ("%s", _("Unsupported file version, or probably not an INFO2 file at all."));
            g_printerr ("\n");
            ret = R2_ERR_BROKEN_FILE;
            goto validation_broken;
        }
        break;

      default:
        ret = R2_ERR_BROKEN_FILE;
        goto validation_broken;
    }

    rewind (fp);
    *infile = fp;
    meta->version = (uint64_t) ver;

    return R2_OK;

  validation_broken:

    fclose (fp);
    return ret;
}


static rbin_struct *
populate_record_data (void     *buf,
                      gsize     bufsize,
                      gboolean *junk_detected)
{
    rbin_struct    *record;
    uint32_t        drivenum;
    size_t          read;
    char           *legacy_fname;

    record = g_malloc0 (sizeof (rbin_struct));

    /* Guarantees null-termination by allocating extra byte; same goes with
     * unicode filename */
    legacy_fname = g_malloc0 (RECORD_INDEX_OFFSET - LEGACY_FILENAME_OFFSET + 1);
    copy_field (legacy_fname, LEGACY_FILENAME_OFFSET, RECORD_INDEX_OFFSET);

    /* Index number associated with the record */
    copy_field (&record->index_n, RECORD_INDEX_OFFSET, DRIVE_LETTER_OFFSET);
    record->index_n = GUINT32_FROM_LE (record->index_n);
    g_debug ("index=%u", record->index_n);

    /* Number representing drive letter */
    copy_field (&drivenum, DRIVE_LETTER_OFFSET, FILETIME_OFFSET);
    drivenum = GUINT32_FROM_LE (drivenum);
    g_debug ("drive=%u", drivenum);
    if (drivenum >= sizeof (driveletters) - 1)
        g_warning (_("Invalid drive number (0x%X) for record %u."),
                   drivenum, record->index_n);
    record->drive = driveletters[MIN (drivenum, sizeof (driveletters) - 1)];

    record->gone = FILESTATUS_EXISTS;
    /* first byte will be removed from filename if file is not in recycle bin */
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

    if (bufsize == LEGACY_RECORD_SIZE)
        return record;

    /* Part below deals with unicode path only */

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
parse_record_cb (char *index_file,
                 metarecord *meta)
{
    rbin_struct   *record;
    FILE          *infile;
    gsize          read_sz, record_sz;
    void          *buf = NULL;

    exit_status = validate_index_file (index_file, &infile, meta);
    if ( exit_status != R2_OK )
    {
        g_printerr (_("File '%s' fails validation."), index_file);
        g_printerr ("\n");
        return;
    }

    g_debug ("Start populating record for '%s'...", index_file);

    record_sz = meta->recordsize;
    buf = g_malloc0 (record_sz);

    fseek (infile, RECORD_START_OFFSET, SEEK_SET);

    while (record_sz == (read_sz = fread (buf, 1, record_sz, infile)))
    {
        record = populate_record_data (buf, record_sz, &meta->fill_junk);
        g_ptr_array_add (meta->records, record);
    }
    g_free (buf);

    if ( ferror (infile) )
    {
        g_critical (_("Failed to read record at position %li: %s"),
                   ftell (infile), strerror (errno));
        exit_status = R2_ERR_OPEN_FILE;
    }
    if ( feof (infile) && read_sz && ( read_sz < record_sz ) )
    {
        g_warning (_("Premature end of file, last record (%zu bytes) discarded"), read_sz);
        exit_status = R2_ERR_BROKEN_FILE;
    }

    fclose (infile);
}

int
main (int    argc,
      char **argv)
{
    GError *error = NULL;

    UNUSED (argc);

    exit_status = rifiuti_init (
        RECYCLE_BIN_TYPE_FILE,
        N_("INFO2"),
        N_("Parse INFO2 file and dump recycle bin data."),
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

    if (!dump_content (&error))
        exit_status = R2_ERR_WRITE_FILE;

    cleanup:

    switch (exit_status)
    {
        case R2_ERR_USER_ENCODING:
        if (legacy_encoding) {
            g_printerr (_("Some entries could not be interpreted in %s encoding."
                "  The concerned characters are displayed in hex value instead."
                "  Very likely the (localised) Windows generating the recycle bin "
                "artifact does not use specified codepage."), legacy_encoding);
        } else {
            g_printerr ("%s", _("Some entries could not be presented as correct "
                "unicode path.  The concerned characters are displayed "
                "in escaped unicode sequences."));
        }
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
