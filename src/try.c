#include <glib.h>

int
main (int argc,
      char **argv)
{
    const char *cmd = "whoami.exe /user /fo csv";
    // const char *cmd = "whoami";
    char *out = NULL, *error = NULL;
    int status;
    GError *error = NULL;

    gboolean rs = g_spawn_command_line_sync(
            cmd, &out, &error, &status, &error);

    g_print("result = %d\n", (int)rs);
    g_print("out = %s\n", out ? g_strchomp (out) : "no out");
    g_print("error = %s\n", error ? g_strchomp (error) : "no error");
    g_print("sts = %d\n", status);
    if (error != NULL) {
        g_print("GEr = %s\n", error->message);
        g_print("Quark = %s\n", g_quark_to_string(error->domain));
    }

    g_free (out);
    g_free (error);
    g_clear_error (&error);
    return EXIT_SUCCESS;
}
