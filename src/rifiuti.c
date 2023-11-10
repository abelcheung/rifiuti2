/*
 * Copyright (C) 2003, Keith J. Jones.
 * Copyright (C) 2007-2023, Abel Cheung.
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

    copy_field (&ver, VERSION, KEPT_ENTRY);
    ver = GUINT32_FROM_LE (ver);

    /* total_entry only meaningful for 95 and NT4, on other versions
     * it's junk memory data, don't bother copying */
    if ( ( ver == VERSION_NT4 ) || ( ver == VERSION_WIN95 ) ) {
        copy_field (&meta.total_entry, TOTAL_ENTRY, RECORD_SIZE);
        meta.total_entry = GUINT32_FROM_LE (meta.total_entry);
    }

    copy_field (&meta.recordsize, RECORD_SIZE, FILESIZE_SUM);
    meta.recordsize = GUINT32_FROM_LE (meta.recordsize);

    g_free (buf);

    /* Turns out version is not reliable indicator. Use size instead */
    switch (meta.recordsize)
    {
      case LEGACY_RECORD_SIZE:

        meta.has_unicode_path = FALSE;

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

        meta.has_unicode_path = TRUE;
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
    meta.version = (int64_t) ver;

    return EXIT_SUCCESS;

  validation_broken:

    fclose (fp);
    return ret;
}


static rbin_struct *
populate_record_data (void *buf)
{
    rbin_struct    *record;
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
    copy_field (&record->winfiletime, FILETIME, FILESIZE);
    record->winfiletime = GINT64_FROM_LE (record->winfiletime);
    record->deltime = win_filetime_to_gdatetime (record->winfiletime);

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

    meta.is_empty = TRUE;
    while (meta.recordsize == (size = fread (buf, 1, meta.recordsize, infile)))
    {
        record = populate_record_data (buf);
        record->meta = &meta;
        /* INFO2 already sort entries by time */
        *recordlist = g_slist_append (*recordlist, record);
        meta.is_empty = FALSE;
    }
    g_free (buf);

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

    exit_status = check_file_args (fileargs[0], &filelist, RECYCLE_BIN_TYPE_FILE);
    if (exit_status != EXIT_SUCCESS)
        goto cleanup;

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
            g_printerr ("%s", _("Some entries could not be presented as correct "
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

    close_error_handle ();

    return exit_status;
}
