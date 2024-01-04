/*
 * Copyright (C) 2019-2024, Abel Cheung
 * rifiuti2 is released under Revised BSD License.
 * Please see LICENSE file for more info.
 */

#include <stdbool.h>
#include <glib.h>

bool conv_established (char *enc)
{
    GIConv cd = g_iconv_open ("UTF-8", enc);

    if (cd != (GIConv) -1) {
        g_iconv_close (cd);
        return true;
    }
    return false;
}

int main (int argc, char **argv)
{
    char *enc;

    for (int i = 1; i < argc; i++) {
        enc = argv[i];
        if (*enc == '\0')
            continue;
        if (conv_established (enc)) {
            g_print("%s\n", enc);
            return 0;
        }
    }
    return 1;
}
