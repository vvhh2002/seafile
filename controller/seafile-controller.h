/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * Seafile-controller is responsible for: 
 *
 *    1. Start: start server processes:
 *
 *       - ccnet-server
 *       - seaf-server
 *       - seaf-mon
 *
 *    2. Repair:
 *
 *       - ensure ccnet process availability by watching client->connfd
 *       - ensure server processes availablity by receiving heartbeat
 *         messages.
 *         If heartbeat messages for some process is not received for a given
 *         time, try to restart it.
 *      
 */

#ifndef SEAFILE_CONTROLLER_H
#define SEAFILE_CONTROLLER_H

typedef struct _SeafileController SeafileController;

struct _SeafileController {
    char *ccnet_dir;
    char *seafile_dir;
    
    struct _CcnetClient *client;
    guint               check_timer;
    /* Decide whether to start seaf-server in cloud mode  */
    gboolean            cloud_mode;

    struct _SeafApplication *ccnet_app;
    GList               *applications;
};
#endif
