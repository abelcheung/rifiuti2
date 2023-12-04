#include <glib.h>

int
main (int argc,
      char **argv)
{
    const char *cmd = "whoami.exe /user /fo csv";
    // const char *cmd = "whoami";
    char *out = NULL, *err = NULL;
    int status;
    GError *error = NULL;

    gboolean rs = g_spawn_command_line_sync(
            cmd, &out, &err, &status, &error);

    g_print("result = %d\n", (int)rs);
    g_print("out = %s\n", out ? g_strchomp (out) : "no out");
    g_print("err = %s\n", err ? g_strchomp (err) : "no err");
    g_print("sts = %d\n", status);
    if (error != NULL) {
        g_print("GEr = %s\n", error->message);
        g_print("Quark = %s\n", g_quark_to_string(error->domain));
    }

    g_free (out);
    g_free (err);
    g_clear_error (&error);
    return EXIT_SUCCESS;
}
