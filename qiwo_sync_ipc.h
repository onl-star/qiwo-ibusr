#ifndef QIWO_SYNC_IPC_H_
#define QIWO_SYNC_IPC_H_

#include <glib.h>

G_BEGIN_DECLS

gchar *qiwo_sync_ipc_socket_path(void);
gint qiwo_sync_ipc_create_server(GError **error);
gboolean qiwo_sync_ipc_request_sync(gchar **response, GError **error);

G_END_DECLS

#endif  // QIWO_SYNC_IPC_H_
