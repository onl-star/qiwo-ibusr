#include "qiwo_sync_ipc.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define QIWO_SYNC_IPC_REQUEST "SYNC\n"

gchar *
qiwo_sync_ipc_socket_path(void)
{
  const gchar *runtime_dir = g_get_user_runtime_dir();
  if (runtime_dir && runtime_dir[0]) {
    return g_build_filename(runtime_dir, "qiwo-ibus-rime-sync.sock", NULL);
  }
  return g_strdup_printf("%s/qiwo-ibus-rime-sync-%u.sock",
                         g_get_tmp_dir(), (guint)getuid());
}

static gboolean
set_errno_error(GError **error, const gchar *message, const gchar *path)
{
  gint saved_errno = errno;
  g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved_errno),
              "%s%s%s: %s",
              message,
              path ? ": " : "",
              path ? path : "",
              g_strerror(saved_errno));
  return FALSE;
}

static gboolean
write_all(gint fd, const gchar *data, gsize length, GError **error)
{
  gsize offset = 0;
  while (offset < length) {
#ifdef MSG_NOSIGNAL
    ssize_t written = send(fd, data + offset, length - offset, MSG_NOSIGNAL);
#else
    ssize_t written = write(fd, data + offset, length - offset);
#endif
    if (written < 0) {
      if (errno == EINTR) continue;
      return set_errno_error(error, "Unable to write sync IPC request", NULL);
    }
    if (written == 0) {
      g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                          "Unable to write sync IPC request.");
      return FALSE;
    }
    offset += (gsize)written;
  }
  return TRUE;
}

gint
qiwo_sync_ipc_create_server(GError **error)
{
  g_autofree gchar *path = qiwo_sync_ipc_socket_path();
  if (strlen(path) >= sizeof(((struct sockaddr_un *)0)->sun_path)) {
    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NAMETOOLONG,
                "Sync IPC socket path is too long: %s", path);
    return -1;
  }

  gint fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    set_errno_error(error, "Unable to create sync IPC socket", NULL);
    return -1;
  }

  unlink(path);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  g_strlcpy(addr.sun_path, path, sizeof(addr.sun_path));

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    set_errno_error(error, "Unable to bind sync IPC socket", path);
    close(fd);
    return -1;
  }
  chmod(path, 0600);

  if (listen(fd, 4) != 0) {
    set_errno_error(error, "Unable to listen on sync IPC socket", path);
    close(fd);
    unlink(path);
    return -1;
  }

  return fd;
}

gboolean
qiwo_sync_ipc_request_sync(gchar **response, GError **error)
{
  if (response) {
    *response = NULL;
  }

  g_autofree gchar *path = qiwo_sync_ipc_socket_path();
  if (strlen(path) >= sizeof(((struct sockaddr_un *)0)->sun_path)) {
    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NAMETOOLONG,
                "Sync IPC socket path is too long: %s", path);
    return FALSE;
  }

  gint fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return set_errno_error(error, "Unable to create sync IPC socket", NULL);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  g_strlcpy(addr.sun_path, path, sizeof(addr.sun_path));

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    set_errno_error(error,
                    "Unable to contact Qiwo IBus engine. Switch to Qiwo input method and try again",
                    path);
    close(fd);
    return FALSE;
  }

  if (!write_all(fd, QIWO_SYNC_IPC_REQUEST,
                 strlen(QIWO_SYNC_IPC_REQUEST), error)) {
    close(fd);
    return FALSE;
  }
  shutdown(fd, SHUT_WR);

  GString *buffer = g_string_new("");
  gchar chunk[4096];
  for (;;) {
    ssize_t n = read(fd, chunk, sizeof(chunk));
    if (n < 0) {
      if (errno == EINTR) continue;
      g_string_free(buffer, TRUE);
      set_errno_error(error, "Unable to read sync IPC response", NULL);
      close(fd);
      return FALSE;
    }
    if (n == 0) break;
    g_string_append_len(buffer, chunk, n);
  }
  close(fd);

  if (g_str_has_prefix(buffer->str, "OK\n")) {
    gchar *body = g_strdup(buffer->str + 3);
    g_strstrip(body);
    if (response) {
      *response = body;
    } else {
      g_free(body);
    }
    g_string_free(buffer, TRUE);
    return TRUE;
  }

  if (g_str_has_prefix(buffer->str, "ERROR\n")) {
    g_autofree gchar *body = g_strdup(buffer->str + 6);
    g_strstrip(body);
    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                "%s", body[0] ? body : "Qiwo IBus engine sync failed.");
  } else {
    g_autofree gchar *body = g_string_free(buffer, FALSE);
    g_strstrip(body);
    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                "Invalid sync IPC response: %s",
                body[0] ? body : "(empty)");
    return FALSE;
  }

  g_string_free(buffer, TRUE);
  return FALSE;
}
