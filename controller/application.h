#ifndef SEAF_APPLICATION_H
#define SEAF_APPLICATION_H

typedef struct _SeafApplication SeafApplication;

struct _SeafApplication {
    char *name;
    char **argv;
    char *pidfile;
    char *workdir;
};

gboolean is_application_alive (SeafApplication *application);
    
int start_application (SeafApplication *application);

void terminate_application(SeafApplication *application);

#endif
