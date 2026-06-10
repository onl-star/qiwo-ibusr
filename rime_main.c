// ibus-rime program entry

#include "rime_config.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <ibus.h>
#include <libnotify/notify.h>
#include <rime_api.h>
#include "rime_engine.h"
#include "rime_settings.h"
#include "qiwo_rime_default_config.h"
#include "qiwo_sync_command.h"
#include "qiwo_sync_ipc.h"
#include "qiwo_webdav_config.h"

// TODO:
#define _(x) (x)

#define DISTRIBUTION_NAME _("齐我输入法")
#define DISTRIBUTION_CODE_NAME "qiwo"
#define DISTRIBUTION_VERSION QIWO_IBUS_VERSION
#define IBUS_COMPONENT_NAME "im.rime.Qiwo"

RimeApi *rime_api = NULL;
static gint sync_ipc_fd = -1;
static guint sync_ipc_watch_id = 0;

static const char* get_ibus_rime_user_data_dir(char *path) {
  const char* home = getenv("HOME");
  strcpy(path, home);
  strcat(path, "/.config/ibus/rime");
  return path;
}

static void show_message(const char* summary, const char* details) {
  NotifyNotification* notice = notify_notification_new(summary, details, NULL);
  notify_notification_show(notice, NULL);
  g_object_unref(notice);
}

static void notification_handler(void* context_object,
                                 RimeSessionId session_id,
                                 const char* message_type,
                                 const char* message_value) {
  if (!strcmp(message_type, "deploy")) {
    if (!strcmp(message_value, "start")) {
      show_message(_("Rime is under maintenance ..."), NULL);
    }
    else if (!strcmp(message_value, "success")) {
      show_message(_("Rime is ready."), NULL);
      ibus_rime_load_settings();
    }
    else if (!strcmp(message_value, "failure")) {
      show_message(_("Rime has encountered an error."),
                   _("See /tmp/rime.ibus.ERROR for details."));
    }
    return;
  }
}

static void fill_traits(RimeTraits *traits) {
  traits->shared_data_dir = IBUS_RIME_SHARED_DATA_DIR;
  traits->distribution_name = DISTRIBUTION_NAME;
  traits->distribution_code_name = DISTRIBUTION_CODE_NAME;
  traits->distribution_version = DISTRIBUTION_VERSION;
  traits->app_name = "rime.qiwo";
}

void ibus_rime_start(gboolean full_check) {
  char user_data_dir[512] = {0};
  get_ibus_rime_user_data_dir(user_data_dir);
  if (!g_file_test(user_data_dir, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(user_data_dir, 0700);
  }
  gboolean default_config_created = FALSE;
  g_autoptr(GError) default_config_error = NULL;
  if (!qiwo_rime_default_config_ensure(user_data_dir, &default_config_created,
                                       &default_config_error)) {
    g_warning("error ensuring Qiwo default Rime schema: %s",
              default_config_error ? default_config_error->message : "unknown");
  }

  RIME_STRUCT(RimeTraits, ibus_rime_traits);
  fill_traits(&ibus_rime_traits);
  ibus_rime_traits.user_data_dir = user_data_dir;

  rime_api->initialize(&ibus_rime_traits);
  if (rime_api->start_maintenance((Bool)(full_check || default_config_created))) {
    // update frontend config
    rime_api->deploy_config_file("ibus_rime.yaml", "config_version");
  }
}

void ibus_rime_stop() {
  if (rime_api) {
    rime_api->finalize();
  }
}

static void ibus_disconnect_cb(IBusBus *bus, gpointer user_data) {
  g_debug("bus disconnected");
  ibus_quit();
}

static gboolean auto_sync_callback(gpointer user_data);
static void start_sync_ipc_server(void);
static void stop_sync_ipc_server(void);

static void rime_with_ibus() {
  ibus_init();
  IBusBus *bus = ibus_bus_new();
  g_object_ref_sink(bus);

  if (!ibus_bus_is_connected(bus)) {
    g_warning("not connected to ibus");
    exit(0);
  }

  g_signal_connect(bus, "disconnected", G_CALLBACK(ibus_disconnect_cb), NULL);

  IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus));
  g_object_ref_sink(factory);

  ibus_factory_add_engine(factory, "qiwo", IBUS_TYPE_RIME_ENGINE);
  if (!ibus_bus_request_name(bus, IBUS_COMPONENT_NAME, 0)) {
    g_error("error requesting bus name");
    exit(1);
  }

  if (!notify_init("qiwo")) {
    g_error("notify_init failed");
    exit(1);
  }
  rime_api->set_notification_handler(notification_handler, NULL);

  RIME_STRUCT(RimeTraits, ibus_rime_traits);
  fill_traits(&ibus_rime_traits);
  rime_api->setup(&ibus_rime_traits);

  gboolean full_check = FALSE;
  ibus_rime_start(full_check);
  ibus_rime_load_settings();

  // 安装自动同步词库定时器
  guint auto_sync_timer_id = 0;
  if (g_ibus_rime_settings.auto_sync_interval_seconds > 0) {
    auto_sync_timer_id = g_timeout_add_seconds(
        g_ibus_rime_settings.auto_sync_interval_seconds,
        auto_sync_callback, NULL);
    g_debug("auto-sync timer installed: %u seconds",
            g_ibus_rime_settings.auto_sync_interval_seconds);
  }

  start_sync_ipc_server();

  ibus_main();

  stop_sync_ipc_server();

  if (auto_sync_timer_id > 0) {
    g_source_remove(auto_sync_timer_id);
  }

  ibus_rime_stop();
  notify_uninit();

  g_object_unref(factory);
  g_object_unref(bus);
}

static void sigterm_cb(int sig) {
  // Notify the main program to exit.
  ibus_quit();
}

static void qiwo_ensure_installation_yaml(const char* user_data_dir,
                                          const char* device_id) {
  char file_path[PATH_MAX];
  snprintf(file_path, sizeof(file_path), "%s/installation.yaml",
           user_data_dir);

  char safe_id[256] = {0};
  const char* src = device_id;
  char* dst = safe_id;
  while (*src && (size_t)(dst - safe_id) < sizeof(safe_id) - 1) {
    char c = g_ascii_tolower(*src);
    if (c == ' ' || c == ':' || c == '\\' || c == '/') c = '-';
    *dst++ = c;
    src++;
  }

  // Ensure sync/ export dir exists
  char sync_dir[PATH_MAX];
  snprintf(sync_dir, sizeof(sync_dir), "%s/sync", user_data_dir);
  g_mkdir_with_parents(sync_dir, 0700);
  snprintf(sync_dir, sizeof(sync_dir), "%s/sync/%s", user_data_dir, safe_id);
  g_mkdir_with_parents(sync_dir, 0700);

  char sync_dir_yaml[PATH_MAX];
  snprintf(sync_dir_yaml, sizeof(sync_dir_yaml), "%s/sync", user_data_dir);

  gchar* content = NULL;
  if (g_file_get_contents(file_path, &content, NULL, NULL)) {
    gchar** lines = g_strsplit(content, "\n", -1);
    GString* updated = g_string_new("");
    gboolean has_sync_dir = FALSE;
    gboolean has_install_id = FALSE;

    for (int i = 0; lines[i]; i++) {
      if (g_str_has_prefix(lines[i], "sync_dir:")) {
        has_sync_dir = TRUE;
        g_string_append_printf(updated, "sync_dir: \"%s\"\n", sync_dir_yaml);
        continue;
      }
      if (g_str_has_prefix(lines[i], "installation_id:")) {
        has_install_id = TRUE;
        g_string_append_printf(updated, "installation_id: \"%s\"\n", safe_id);
      } else {
        g_string_append(updated, lines[i]);
        g_string_append_c(updated, '\n');
      }
    }

    if (!has_sync_dir) {
      g_string_append_printf(updated, "sync_dir: \"%s\"\n", sync_dir_yaml);
    }
    if (!has_install_id) {
      g_string_append_printf(updated, "installation_id: \"%s\"\n", safe_id);
    }

    g_file_set_contents(file_path, updated->str, -1, NULL);
    g_string_free(updated, TRUE);
    g_strfreev(lines);
    g_free(content);
  } else {
    gchar* yaml = g_strdup_printf(
        "distribution: \"Qiwo\"\n"
        "distribution_version: \"1.0\"\n"
        "installation_id: \"%s\"\n"
        "sync_dir: \"%s\"\n",
        safe_id, sync_dir_yaml);
    g_file_set_contents(file_path, yaml, -1, NULL);
    g_free(yaml);
  }
}

static gboolean auto_sync_callback(gpointer user_data) {
  if (g_ibus_rime_settings.auto_sync_interval_seconds == 0)
    return G_SOURCE_REMOVE;

  ibus_rime_sync_user_data();
  return G_SOURCE_CONTINUE;
}

static gboolean
write_all_to_fd(gint fd, const gchar *data, gsize length)
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
      return FALSE;
    }
    if (written == 0) {
      return FALSE;
    }
    offset += (gsize)written;
  }
  return TRUE;
}

static char* get_qiwo_settings_tool(void) {
  static const char* paths[] = {
#ifdef QIWO_SYNC_DIR
    QIWO_SYNC_DIR "/qiwo-webdav-settings",
#endif
    "/usr/bin/qiwo-webdav-settings",
    "/usr/local/bin/qiwo-webdav-settings",
    "/usr/share/qiwo/qiwo-webdav-settings",
    "/usr/local/share/qiwo/qiwo-webdav-settings",
    NULL
  };
  for (int i = 0; paths[i]; i++) {
    if (g_file_test(paths[i], G_FILE_TEST_IS_EXECUTABLE))
      return g_strdup(paths[i]);
  }
  return NULL;
}

void ibus_rime_open_webdav_settings(void) {
  char* tool = get_qiwo_settings_tool();
  if (!tool) {
    show_message(_("Qiwo WebDAV settings unavailable"),
                 _("qiwo-webdav-settings was not found."));
    return;
  }

  gchar* argv[] = { tool, NULL };
  GError* error = NULL;
  if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                     NULL, NULL, NULL, &error)) {
    g_warning("Qiwo WebDAV settings launch failed: %s",
              error ? error->message : "unknown");
    show_message(_("Qiwo WebDAV settings failed"),
                 error ? error->message : _("Unable to launch settings."));
    g_clear_error(&error);
  }
  g_free(tool);
}

static gboolean
rime_sync_user_data_hook(gpointer user_data, GError **error)
{
  (void)user_data;
  if (!rime_api) {
    g_set_error(error, QIWO_SYNC_COMMAND_ERROR,
                QIWO_SYNC_COMMAND_ERROR_RIME_SYNC_FAILED,
                "Rime API is not initialized.");
    return FALSE;
  }

  if (!rime_api->sync_user_data()) {
    g_set_error(error, QIWO_SYNC_COMMAND_ERROR,
                QIWO_SYNC_COMMAND_ERROR_RIME_SYNC_FAILED,
                "Rime sync_user_data failed.");
    return FALSE;
  }
  rime_api->join_maintenance_thread();
  return TRUE;
}

static gboolean
ibus_rime_redeploy_after_sync(GError **error)
{
  if (!rime_api) {
    g_set_error(error, QIWO_SYNC_COMMAND_ERROR,
                QIWO_SYNC_COMMAND_ERROR_RIME_SYNC_FAILED,
                "Rime API is not initialized.");
    return FALSE;
  }

  if (!rime_api->start_maintenance((Bool)TRUE)) {
    g_set_error_literal(error, QIWO_SYNC_COMMAND_ERROR,
                        QIWO_SYNC_COMMAND_ERROR_RIME_SYNC_FAILED,
                        "Rime redeploy did not start.");
    return FALSE;
  }
  rime_api->deploy_config_file("ibus_rime.yaml", "config_version");
  rime_api->join_maintenance_thread();
  ibus_rime_load_settings();
  return TRUE;
}

static gboolean
ibus_rime_run_webdav_sync(gchar **message_out)
{
  if (message_out) {
    *message_out = NULL;
  }

  char user_data_dir[PATH_MAX];
  get_ibus_rime_user_data_dir(user_data_dir);

  QiwoEffectiveWebDavSettings settings;
  qiwo_effective_webdav_settings_init(&settings);
  GError* error = NULL;
  if (!qiwo_webdav_config_load_effective(&settings, &error)) {
    g_warning("Qiwo WebDAV config load failed: %s",
              error ? error->message : "unknown");
    if (message_out) {
      *message_out = g_strdup(error ? error->message :
                              _("Unable to load settings."));
    }
    g_clear_error(&error);
    qiwo_effective_webdav_settings_clear(&settings);
    return FALSE;
  }
  if (!settings.device_id || !settings.device_id[0]) {
    g_free(settings.device_id);
    settings.device_id = g_strdup(g_get_host_name());
  }
  g_debug("Qiwo WebDAV panel sync uses effective settings");

  // Ensure installation.yaml sync config
  qiwo_ensure_installation_yaml(user_data_dir, settings.device_id);

  QiwoSyncCommandResult result;
  qiwo_sync_command_result_init(&result);
  gboolean ok = qiwo_sync_command_run_full_sync(
      user_data_dir,
      &settings,
      rime_sync_user_data_hook,
      rime_sync_user_data_hook,
      NULL,
      &result,
      &error);

  if (!ok) {
    const gchar* details = error ? error->message :
        (result.stderr_text ? result.stderr_text : _("Unknown error."));
    g_warning("Qiwo WebDAV sync failed: %s", details);
    if (message_out) {
      *message_out = g_strdup(details);
    }
  } else {
    GError *deploy_error = NULL;
    ok = ibus_rime_redeploy_after_sync(&deploy_error);
    if (!ok) {
      const gchar *details = deploy_error ? deploy_error->message :
          _("Unknown error.");
      g_warning("Qiwo Rime redeploy failed after WebDAV sync: %s", details);
      if (message_out) {
        *message_out = g_strdup_printf(
            "Sync completed, but Rime redeploy failed: %s", details);
      }
      g_clear_error(&deploy_error);
    } else if (message_out) {
      g_autofree gchar *summary = g_strdup(
          result.stdout_text && result.stdout_text[0] ?
          result.stdout_text : _("Sync completed."));
      g_strstrip(summary);
      *message_out = g_strdup_printf(
          "%s\n%s",
          summary[0] ? summary : _("Sync completed."),
          _("Rime redeployed."));
    }
  }

  g_clear_error(&error);
  qiwo_sync_command_result_clear(&result);
  qiwo_effective_webdav_settings_clear(&settings);
  return ok;
}

void ibus_rime_sync_user_data(void) {
  gchar *message = NULL;
  gboolean ok = ibus_rime_run_webdav_sync(&message);
  show_message(ok ? _("Qiwo WebDAV sync complete") :
                    _("Qiwo WebDAV sync failed"),
               message && message[0] ? message :
               (ok ? _("Sync completed.") : _("Unknown error.")));
  g_free(message);
}

static gboolean
sync_ipc_accept_cb(GIOChannel *source, GIOCondition condition, gpointer user_data)
{
  (void)source;
  (void)user_data;
  if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) {
    return G_SOURCE_CONTINUE;
  }

  gint client_fd = accept(sync_ipc_fd, NULL, NULL);
  if (client_fd < 0) {
    return G_SOURCE_CONTINUE;
  }

  gchar *message = NULL;
  gboolean ok = ibus_rime_run_webdav_sync(&message);
  g_autofree gchar *response =
      g_strdup_printf("%s\n%s\n",
                      ok ? "OK" : "ERROR",
                      message && message[0] ? message :
                      (ok ? _("Sync completed.") : _("Unknown error.")));
  write_all_to_fd(client_fd, response, strlen(response));
  close(client_fd);
  g_free(message);
  return G_SOURCE_CONTINUE;
}

static void
start_sync_ipc_server(void)
{
  GError *error = NULL;
  sync_ipc_fd = qiwo_sync_ipc_create_server(&error);
  if (sync_ipc_fd < 0) {
    g_warning("Qiwo sync IPC server failed: %s",
              error ? error->message : "unknown");
    g_clear_error(&error);
    return;
  }

  GIOChannel *channel = g_io_channel_unix_new(sync_ipc_fd);
  sync_ipc_watch_id = g_io_add_watch(
      channel, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
      sync_ipc_accept_cb, NULL);
  g_io_channel_unref(channel);
}

static void
stop_sync_ipc_server(void)
{
  if (sync_ipc_watch_id > 0) {
    g_source_remove(sync_ipc_watch_id);
    sync_ipc_watch_id = 0;
  }
  if (sync_ipc_fd >= 0) {
    close(sync_ipc_fd);
    sync_ipc_fd = -1;
  }
  g_autofree gchar *path = qiwo_sync_ipc_socket_path();
  unlink(path);
}

int main(gint argc, gchar** argv) {
  signal(SIGTERM, sigterm_cb);
  signal(SIGINT, sigterm_cb);

  rime_api = rime_get_api();
  rime_with_ibus();
  return 0;
}
