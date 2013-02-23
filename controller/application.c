#include "common.h"

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#include "log.h"
#include "application.h"


/* returns the pid of the newly created process */
static int
spawn_process (char *argv[], const char *workdir)
{
    char **ptr = argv;
    GString *buf = g_string_new(argv[0]);
    while (*(++ptr)) {
        g_string_append_printf (buf, " %s", *ptr);
    }
    seaf_message ("spawn_process: %s\n", buf->str);
    g_string_free (buf, TRUE);

    pid_t pid = fork();

    if (pid == 0) {
        /* child process */
        if (workdir != NULL) {
            if (chdir(workdir) < 0) {
                seaf_warning ("chdir to %s failed: %s\n", workdir, strerror(errno));
                exit(-1);
            }
        }
        execvp (argv[0], argv);
        seaf_warning ("failed to execvp %s\n", argv[0]);
        exit(-1);
    } else {
        /* controller */
        if (pid == -1)
            seaf_warning ("error when fork %s: %s\n", argv[0], strerror(errno));

        return (int)pid;
    }
}

static int
read_pid_from_pidfile (const char *pidfile)
{
    FILE *pf = fopen (pidfile, "r");
    if (!pf) {
        return -1;
    }

    int pid = -1;
    if (fscanf (pf, "%d", &pid) < 0) {
        seaf_warning ("bad pidfile format: %s\n", pidfile);
        return -1;
    }

    return pid;
}

gboolean
is_application_alive (SeafApplication *app)
{
    int pid = read_pid_from_pidfile(app->pidfile);
    if (pid < 0) {
        return FALSE;
    }

    char proc_dir[32];
    snprintf (proc_dir, sizeof(proc_dir), "/proc/%d", pid);

    return g_file_test(proc_dir, G_FILE_TEST_IS_DIR);
}

int
start_application (SeafApplication *app)
{
    int ret = spawn_process (app->argv, app->workdir);
    if (ret < 0) {
        return -1;
    } else {
        seaf_message ("spawned %s\n", app->name);
        return 0;
    }
}

void
terminate_application(SeafApplication *app)
{
    int pid = read_pid_from_pidfile(app->pidfile);
    if (pid < 0) {
        return;
    }

    kill((pid_t)pid, SIGTERM);
}
