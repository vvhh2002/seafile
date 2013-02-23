/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "common.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <getopt.h>

#include <ccnet/ccnet-client.h>

#include "log.h"
#include "application.h"
#include "seafile-controller.h"

#define CHECK_APPLICATIONS_INTERVAL 5 /* every 5 seconds */

static int seaf_controller_start();

static SeafileController *ctl = NULL;

static char *ccnet_debug_level_str = "info";
static char *seafile_debug_level_str = "debug";
static char *office_converter_dir = NULL;
static char *seafevents_dir = NULL;

static void
controller_exit (int code)
{
    if (code != 0) {
        fprintf (stderr, "[ERROR] seaf-controller exited with code %d\n", code);
    }
    exit(code);
}

static char *
get_app_pidfile (const char *app_name)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "/tmp/seafile-%s.pid", app_name);
    return g_strdup(buf);
}

static void
terminate_all_applications()
{
    terminate_application (ctl->ccnet_app);
    GList *ptr = ctl->applications;
    for (ptr = ctl->applications; ptr != NULL; ptr = ptr->next) {
        SeafApplication *app = (SeafApplication *)ptr->data;
        terminate_application(app);
    }
}

static gboolean
check_all_applications (void *data)
{
    if (!is_application_alive(ctl->ccnet_app)) {
        /* When ccnet is dead,
         *
         * 1. Restarts all components
         * 2. This check timer is disabled, and enabled again when all components are started
         */
        seaf_message ("ccnet-server is dead, restart all\n");
        if (seaf_controller_start() < 0) {
            controller_exit(1);
        }
        return FALSE;
    }

    GList *ptr = ctl->applications;
    for (ptr = ctl->applications; ptr != NULL; ptr = ptr->next) {
        SeafApplication *app = (SeafApplication *)ptr->data;
        if (!is_application_alive(app)) {
            seaf_message ("%s is dead, restart it\n", app->name);
            start_application(app);
        }
    }

    return TRUE;
}

static char **
copy_argv (char *argv[])
{
    char **ptr;
    char **ret;
    int i, len = 0;
    for (ptr = argv; *ptr; ptr++) {
        len++;
    }

    len++;

    ret = g_new0 (char *, len);
    for (i = 0; i < len; i++) {
        ret[i] = g_strdup(argv[i]);
    }

    return ret;
}

static SeafApplication *
init_ccnet_server()
{
    SeafApplication *app = g_new0 (SeafApplication, 1);
    app->name = "ccnet-server";
    app->pidfile = get_app_pidfile (app->name);

    char *argv[] = {
        "ccnet-server",
        "-c",           ctl->ccnet_dir,
        "-d",           ctl->seafile_dir,
        "-P",           app->pidfile,
        "--debug",      ccnet_debug_level_str,
        NULL};

    app->argv = copy_argv(argv);

    return app;
}

static SeafApplication *
init_seaf_server()
{
    SeafApplication *app = g_new0 (SeafApplication, 1);
    app->name = "seaf-server";
    app->pidfile = get_app_pidfile (app->name);

    char *argv[] = {
        "seaf-server",
        "-c",           ctl->ccnet_dir,
        "-d",           ctl->seafile_dir,
        "-P",           app->pidfile,
        "--ccnet-debug-level",   seafile_debug_level_str,
        "--seafile-debug-level", ccnet_debug_level_str,
        "-C",
        NULL};

    if (!ctl->cloud_mode) {
        argv[11] = NULL;
    }

    app->argv = copy_argv(argv);

    return app;
}

static SeafApplication *
init_seaf_monitor()
{
    SeafApplication *app = g_new0 (SeafApplication, 1);
    app->name = "seaf-mon";
    app->pidfile = get_app_pidfile (app->name);

    char *argv[] = {
        "seaf-mon",
        "-c", ctl->ccnet_dir,
        "-d", ctl->seafile_dir,
        "-P", app->pidfile,
        NULL};

    app->argv = copy_argv(argv);

    return app;
}

static SeafApplication *
init_httpserver()
{
    SeafApplication *app = g_new0 (SeafApplication, 1);
    app->name = "seaf-httpserver";
    app->pidfile = get_app_pidfile (app->name);

    char *argv[] = {
        "httpserver",
        "-c", ctl->ccnet_dir,
        "-d", ctl->seafile_dir,
        "-P", app->pidfile,
        NULL};

    app->argv = copy_argv(argv);

    return app;
}

static SeafApplication *
init_seafevents()
{
    SeafApplication *app = g_new0 (SeafApplication, 1);
    app->name = "seaf-events";
    app->pidfile = get_app_pidfile (app->name);

    char *logfile = g_build_filename (ctl->seafile_dir, "seaf-events.log", NULL);

    char *argv[] = {
        "python",
        "events.py",
        "-c", ctl->ccnet_dir,
        "-P", app->pidfile,
        "--logfile", logfile,
        NULL};

    app->argv = copy_argv(argv);
    app->workdir = seafevents_dir;

    return app;
}

static SeafApplication *
init_office_converter()
{
    SeafApplication *app = g_new0 (SeafApplication, 1);
    app->name = "seaf-office-converter";
    app->pidfile = get_app_pidfile (app->name);

    char *logfile = g_build_filename (ctl->seafile_dir, "office-convert.log", NULL);

    char *argv[] = {
        "python",
        "app.py",
        "-P", app->pidfile,
        "--logfile", logfile,
        NULL};

    app->argv = copy_argv(argv);
    app->workdir = office_converter_dir;

    return app;
}

static void
init_applications()
{
    SeafApplication *ccnet = init_ccnet_server();
    SeafApplication *seaf_server = init_seaf_server();
    SeafApplication *seaf_mon = init_seaf_monitor();
    SeafApplication *httpserver = init_httpserver();

    ctl->ccnet_app = ccnet;
    ctl->applications = g_list_append(ctl->applications, seaf_server);
    ctl->applications = g_list_append(ctl->applications, seaf_mon);
    ctl->applications = g_list_append(ctl->applications, httpserver);

    if (seafevents_dir) {
        SeafApplication *seaf_events = init_seafevents();
        ctl->applications = g_list_append(ctl->applications, seaf_events);
    }

    if (office_converter_dir) {
        SeafApplication *office_converter = init_office_converter();
        ctl->applications = g_list_append(ctl->applications, office_converter);
    }
}

static int
seaf_controller_init (SeafileController *ctl,
                      char *ccnet_dir, char *seafile_dir,
                      gboolean cloud_mode)
{
    if (!g_file_test (ccnet_dir, G_FILE_TEST_IS_DIR)) {
        seaf_warning ("invalid ccnet dir: %s\n", ccnet_dir);
        return -1;
    }

    if (!g_file_test (seafile_dir, G_FILE_TEST_IS_DIR)) {
        seaf_warning ("invalid seafile dir: %s\n", seafile_dir);
        return -1;
    }

    if (seafevents_dir != NULL
        && !g_file_test(seafevents_dir, G_FILE_TEST_IS_DIR)) {
        seaf_warning ("invalid seafevents dir: %s\n", seafevents_dir);
        return -1;
    }

    if (office_converter_dir != NULL
        && !g_file_test(office_converter_dir, G_FILE_TEST_IS_DIR)) {
        seaf_warning ("invalid office converter dir: %s\n", office_converter_dir);
        return -1;
    }

    /* init applications */
    ctl->ccnet_dir = ccnet_dir;
    ctl->seafile_dir = seafile_dir;
    ctl->cloud_mode = cloud_mode;

    ctl->client = ccnet_client_new();
    if (ccnet_client_load_confdir(ctl->client, ccnet_dir) < 0) {
        fprintf (stderr, "failed to read ccnet conf\n");
        return -1;
    }

    init_applications();

    return 0;
}

static void
on_ccnet_connected ()
{
    GList *ptr;
    for (ptr = ctl->applications; ptr != NULL; ptr = ptr->next) {
        SeafApplication *app = ptr->data;
        if (is_application_alive(app))
            continue;
        if (start_application(app) < 0) {
            controller_exit(1);
        }
    }

    ctl->check_timer = g_timeout_add (CHECK_APPLICATIONS_INTERVAL * 1000,
                                      check_all_applications, NULL);
}

static gboolean
do_connect_ccnet ()
{
    CcnetClient *client = ctl->client;

    if (!client->connected) {
        seaf_message ("wait for ccnet server...\n");
        if (ccnet_client_connect_daemon(client, CCNET_CLIENT_SYNC) < 0) {
            /* ccnet server is not ready yet */
            return TRUE;
        }
    }

    /* Close it for next test */
    ccnet_client_disconnect_daemon (ctl->client);

    seaf_message ("connected to ccnet server\n");
    on_ccnet_connected ();

    return FALSE;
}

static int
seaf_controller_start ()
{
    if (start_application(ctl->ccnet_app) < 0) {
        return -1;
    }

    /* Only start other applications when ccnet can be connected */
    g_timeout_add (1000 * 1, do_connect_ccnet, NULL);

    return 0;
}

static void
sigint_handler (int signo)
{
    terminate_all_applications();

    signal (signo, SIG_DFL);
    raise (signo);
}

static void
sigchld_handler (int signo)
{
    waitpid (-1, NULL, WNOHANG);
}

static void
set_signal_handlers ()
{
    signal (SIGINT, sigint_handler);
    signal (SIGTERM, sigint_handler);
    signal (SIGCHLD, sigchld_handler);
    signal (SIGPIPE, SIG_IGN);
}

static void
run_controller_loop ()
{
    GMainLoop *mainloop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (mainloop);
}

void
set_environment_variables()
{
    g_setenv("CCNET_CONF_DIR", ctl->ccnet_dir, TRUE);
    g_setenv("SEAFILE_CONF_DIR", ctl->seafile_dir, TRUE);
}

int main (int argc, char **argv)
{
    char *ccnet_dir = NULL;
    char *seafile_dir = NULL;
    char *logfile = NULL;
    gboolean foreground_mode = FALSE;
    gboolean cloud_mode = FALSE;

    GOptionEntry option_entries[] = {
        { .long_name            = "ccnet-dir",
          .short_name           = 'c',
          .flags                = 0,
          .arg                  = G_OPTION_ARG_STRING,
          .arg_data             = &ccnet_dir,
          .description          = "ccnet dir",
          .arg_description      = NULL },

        { "seafile-dir", 'd', 0, G_OPTION_ARG_STRING,
          &seafile_dir, "seafile dir", NULL },

        { "logfile", 'l', 0, G_OPTION_ARG_STRING,
          &logfile, "log file", NULL },

        { "ccnet-debug-level", 'g', 0, G_OPTION_ARG_STRING,
          &ccnet_debug_level_str, "log level of ccnet", NULL },

        { "seafile-debug-level", 'G', 0, G_OPTION_ARG_STRING,
          &seafile_debug_level_str, "log level of seafile", NULL },

        { "foreground", 'f', 0, G_OPTION_ARG_NONE,
          &foreground_mode, "run in foreground", NULL },

        { "cloud-mode", 'C', 0, G_OPTION_ARG_NONE,
          &cloud_mode, "enable seafile cloud mode", NULL },

        { "seafevents-dir", 0 , 0, G_OPTION_ARG_STRING,
          &seafevents_dir, "seafevents dir", NULL },

        { "office-converter-dir", 0 , 0, G_OPTION_ARG_STRING,
          &office_converter_dir, "office converter dir", NULL },

        { NULL }
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, option_entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        fprintf (stderr, "option parsing failed: %s\n", error->message);
        return -1;
    }

    if (!foreground_mode)
        daemon(1, 0);

    g_type_init ();
#if !GLIB_CHECK_VERSION(2,32,0)
    g_thread_init (NULL);
#endif

    if (!ccnet_dir) {
        fprintf (stderr, "[ccnet dir] must be specified with --ccnet-dir\n");
        controller_exit(1);
    }

    if (!seafile_dir) {
        fprintf (stderr, "[seafile dir] must be specified with --seafile-dir\n");
        controller_exit(1);
    }

    ctl = g_new0 (SeafileController, 1);
    if (seaf_controller_init(ctl, ccnet_dir, seafile_dir, cloud_mode) < 0) {
        controller_exit(1);
    }

    if (!logfile) {
        logfile = g_build_filename (seafile_dir, "controller.log", NULL);
    }

    if (seafile_log_init (logfile, ccnet_debug_level_str,
                          seafile_debug_level_str) < 0) {
        seaf_warning ("Failed to init log.\n");
        controller_exit (1);
    }

    set_signal_handlers ();

    set_environment_variables();

    if (seaf_controller_start () < 0)
        controller_exit (1);

    run_controller_loop ();

    return 0;
}
