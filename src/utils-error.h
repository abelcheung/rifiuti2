/*
 * Copyright (C) 2023, Abel Cheung.
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#pragma once

#include <glib.h>

typedef enum
{
    R2_FATAL_ERROR_LIVE_UNSUPPORTED,  /* Can't detect live system env */
    R2_FATAL_ERROR_ILLEGAL_DATA,  /* all data broken, not empty bin */
    R2_FATAL_ERROR_TEMPFILE,

} R2FatalError;

/**
 * @brief Per record non-fatal error
 * @note Some error may indicate the whole record is invalidated,
 * but there also exists very minor error that doesn't.
 */
typedef enum
{
    R2_REC_ERROR_DRIVE_LETTER,
    R2_REC_ERROR_DUBIOUS_TIME,
    R2_REC_ERROR_DUBIOUS_PATH,
    R2_REC_ERROR_CONV_PATH,
    R2_REC_ERROR_IDX_SIZE_INVALID,
    R2_REC_ERROR_VER_UNSUPPORTED,  /* ($Recycle.bin) bad version */

} R2RecordError;

typedef enum
{
    R2_MISC_ERROR_GET_SID,  // problem getting Security Identifier
    R2_MISC_ERROR_ENUMERATE_MNT,  // can't get usable drive list
} R2MiscError;

// our own error domains

#define R2_FATAL_ERROR (rifiuti_fatal_error_quark ())
GQuark rifiuti_fatal_error_quark (void);

#define R2_REC_ERROR (rifiuti_record_error_quark ())
GQuark rifiuti_record_error_quark (void);

#define R2_MISC_ERROR (rifiuti_misc_error_quark ())
GQuark rifiuti_misc_error_quark (void);

