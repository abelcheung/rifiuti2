#include <stdio.h>
#include <glib.h>

int main()
{
	const char *charset;
	g_get_charset(&charset);
	puts (charset);
}
