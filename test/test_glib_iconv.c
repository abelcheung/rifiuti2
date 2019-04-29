#include <glib.h>

int main (int argc, char **argv)
{
	GIConv cd;

	if (argc < 2 || *(argv[1]) == '\0') {
		return 2;
	}

	/*
	 * In rifiuti2 we only need <legacy> → UTF-8 conversion,
	 * so we only test for this case. There are some odd OSes
	 * where even identity conversion can be unsupported (say
	 * CP936 → CP936), such as on Solaris.
	 */
	cd = g_iconv_open ("UTF-8", argv[1]);

	if (cd != (GIConv) -1)
		g_iconv_close (cd);

	return (cd == (GIConv) -1);
}
