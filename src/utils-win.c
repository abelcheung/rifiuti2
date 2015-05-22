/*
 * Copyright (C) 2015 Abel Cheung.
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
#include "utils-win.h"
#include <windows.h>
#include <glib.h>
#include <glib/gi18n.h>

static char *
convert_with_fallback (const char *string,
                       const char *fallback)
{
	GError *err = NULL;
	char   *output = g_locale_from_utf8 (string, -1, NULL, NULL, &err);
	if (err != NULL)
	{
		g_critical ("Failed to convert message to display: %s\n", err->message);
		g_clear_error (&err);
		return g_strdup (fallback);
	}

	return output;
}

/* GUI message box */
void
gui_message (const char *message)
{
	char *title, *output;

	title = convert_with_fallback (_("This is a command line application"),
	                               "This is a command line application");
	output = convert_with_fallback (message,
	                                "Fail to display help message. Please "
	                                "invoke program with '--help' option.");

	MessageBox (NULL, output, title, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	g_free (title);
	g_free (output);
}

/*
 * A copy of latter part of g_win32_getlocale()
 */
#ifndef SUBLANG_SERBIAN_LATIN_BA
#define SUBLANG_SERBIAN_LATIN_BA 0x06
#endif

static const char *
get_win32_locale_script (int primary,
                         int sub)
{
	switch (primary)
	{
	  case LANG_AZERI:
		switch (sub)
		{
		  case SUBLANG_AZERI_LATIN:    return "@Latn";
		  case SUBLANG_AZERI_CYRILLIC: return "@Cyrl";
		}
		break;

	  case LANG_SERBIAN:		/* LANG_CROATIAN == LANG_SERBIAN */
		switch (sub)
		{
		  case SUBLANG_SERBIAN_LATIN:
		  case SUBLANG_SERBIAN_LATIN_BA: /* Serbian (Latin) - Bosnia and Herzegovina */
			return "@Latn";
		}
		break;
	  case LANG_UZBEK:
		switch (sub)
		{
		  case SUBLANG_UZBEK_LATIN:    return "@Latn";
		  case SUBLANG_UZBEK_CYRILLIC: return "@Cyrl";
		}
		break;
	}
	return NULL;
}

/*
 * We can't use g_win32_getlocale() directly.
 *
 * There are 4 possible source for language UI settings:
 * - GetThreadLocale()  (used by g_win32_getlocale)
 * - Installed language group
 * - User default language
 * - System default language
 *
 * First one is no good because rifiuti2 is a CLI program, where
 * the caller is a console, so this 'language' is merely determined
 * by console code page, which is not an indicator of user preferred
 * language. For example, CP850 can be used by multiple european
 * languages but GetLocaleInfo() always treat it as en_US.
 * Language group is not an indicator too because it can imply
 * several similar languages.
 *
 * So we attempt to use the last 2, and do the dirty work in a manner
 * almost identical to g_win32_getlocale().
 */
char *
get_win32_locale (void)
{
	LCID lcid;
	LANGID langid;
	char *ev;
	char iso639[10];
	char iso3166[10];
	const char *script;

	/* Allow user overriding locale env */
	if (((ev = getenv ("LC_ALL"))      != NULL && ev[0] != '\0') ||
	    ((ev = getenv ("LC_MESSAGES")) != NULL && ev[0] != '\0') ||
	    ((ev = getenv ("LANG"))        != NULL && ev[0] != '\0'))
	return g_strdup (ev);

	lcid = LOCALE_USER_DEFAULT;
	if (!GetLocaleInfo (lcid, LOCALE_SISO639LANGNAME, iso639, sizeof (iso639)) ||
	    !GetLocaleInfo (lcid, LOCALE_SISO3166CTRYNAME, iso3166, sizeof (iso3166)))
	{
		lcid = LOCALE_SYSTEM_DEFAULT;
		if (!GetLocaleInfo (lcid, LOCALE_SISO639LANGNAME, iso639, sizeof (iso639)) ||
			!GetLocaleInfo (lcid, LOCALE_SISO3166CTRYNAME, iso3166, sizeof (iso3166)))
		return g_strdup ("C");
	}

	/* Strip off the sorting rules, keep only the language part.  */
	langid = LANGIDFROMLCID (lcid);

	/* Get script based on language and territory */
	script = get_win32_locale_script (PRIMARYLANGID (langid), SUBLANGID (langid));

	return g_strconcat (iso639, "_", iso3166, script, NULL);
}

/* vim: set sw=4 ts=4 noexpandtab : */
