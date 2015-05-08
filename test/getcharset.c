#include <stdio.h>
#include <locale.h>
#include <glib.h>

int main()
{
	const char *charset;
	setlocale(LC_ALL, "");
	g_get_charset(&charset);
	puts (charset);
}
