// ibus-rime program entry

#include "rime_config.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <ibus.h>
#include <libnotify/notify.h>
#include <rime_api.h>
#include "rime_engine.h"
#include "rime_settings.h"

// TODO:
#define _(x) (x)

#define DISTRIBUTION_NAME _("齐我输入法")
#define DISTRIBUTION_CODE_NAME "qiwo"
#define DISTRIBUTION_VERSION QIWO_IBUS_VERSION

RimeApi *rime_api = NULL;

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
  RIME_STRUCT(RimeTraits, ibus_rime_traits);
  fill_traits(&ibus_rime_traits);
  ibus_rime_traits.user_data_dir = user_data_dir;

  rime_api->initialize(&ibus_rime_traits);
  if (rime_api->start_maintenance((Bool)full_check)) {
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
  if (!ibus_bus_request_name(bus, "im.rime.Rime", 0)) {
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

  ibus_main();

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

static char* get_qiwo_sync_tool(void) {
  static const char* paths[] = {
    QIWO_SYNC_DIR "/qiwo-rime-sync",
    "/usr/bin/qiwo-rime-sync",
    "/usr/local/bin/qiwo-rime-sync",
    "/usr/share/qiwo/qiwo-rime-sync",
    "/usr/local/share/qiwo/qiwo-rime-sync",
    NULL
  };
  for (int i = 0; paths[i]; i++) {
    if (g_file_test(paths[i], G_FILE_TEST_IS_EXECUTABLE))
      return g_strdup(paths[i]);
  }
  return NULL;
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

  gchar* content = NULL;
  if (g_file_get_contents(file_path, &content, NULL, NULL)) {
    gchar** lines = g_strsplit(content, "\n", -1);
    GString* updated = g_string_new("");
    gboolean has_sync_dir = FALSE;
    gboolean has_install_id = FALSE;

    for (int i = 0; lines[i]; i++) {
      if (g_str_has_prefix(lines[i], "sync_dir:")) {
        has_sync_dir = TRUE;
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
      g_string_append(updated, "sync_dir: \"sync\"\n");
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
        "sync_dir: \"sync\"\n",
        safe_id);
    g_file_set_contents(file_path, yaml, -1, NULL);
    g_free(yaml);
  }
}

static gboolean auto_sync_callback(gpointer user_data) {
  if (g_ibus_rime_settings.auto_sync_interval_seconds == 0)
    return G_SOURCE_REMOVE;

  if (rime_api) {
    rime_api->sync_user_data();
  }
  ibus_rime_sync_user_data();
  if (rime_api) {
    rime_api->sync_user_data();
  }
  return G_SOURCE_CONTINUE;
}

void ibus_rime_sync_user_data(void) {
  char* tool = get_qiwo_sync_tool();
  if (!tool) return;

  char user_data_dir[PATH_MAX];
  get_ibus_rime_user_data_dir(user_data_dir);

  // Ensure installation.yaml sync config
  const char* device_id = g_getenv("QIWO_DEVICE_ID");
  if (!device_id) device_id = g_get_host_name();
  qiwo_ensure_installation_yaml(user_data_dir, device_id);

  GString* cmd = g_string_new("\"");
  g_string_append(cmd, tool);
  g_string_append(cmd, "\" sync --frontend ibus-rime --rime-user-dir \"");
  g_string_append(cmd, user_data_dir);
  g_string_append(cmd, "\"");

  const char* url = g_getenv("QIWO_WEBDAV_URL");
  const char* username = g_getenv("QIWO_WEBDAV_USERNAME");
  const char* password_env = g_getenv("QIWO_WEBDAV_PASSWORD");

  if (url && url[0]) {
    g_string_append(cmd, " --remote-url \"");
    g_string_append(cmd, url);
    g_string_append(cmd, "\"");
  }
  if (username && username[0]) {
    g_string_append(cmd, " --username \"");
    g_string_append(cmd, username);
    g_string_append(cmd, "\"");
  }
  if (password_env) {
    g_string_append(cmd, " --password-env QIWO_WEBDAV_PASSWORD");
  }
  if (device_id && device_id[0]) {
    g_string_append(cmd, " --device-id \"");
    g_string_append(cmd, device_id);
    g_string_append(cmd, "\"");
  }

  int exit_code = 0;
  gchar* output = NULL;
  gchar* error_output = NULL;
  GError* error = NULL;

  gboolean ok = g_spawn_command_line_sync(
      cmd->str, &output, &error_output, &exit_code, &error);

  if (!ok || exit_code != 0) {
    g_warning("Qiwo WebDAV sync failed: %s (exit=%d)",
              error ? error->message : (error_output ? error_output : "unknown"),
              exit_code);
  }

  g_free(output);
  g_free(error_output);
  g_clear_error(&error);
  g_string_free(cmd, TRUE);
  g_free(tool);
}

int main(gint argc, gchar** argv) {
  signal(SIGTERM, sigterm_cb);
  signal(SIGINT, sigterm_cb);

  rime_api = rime_get_api();
  rime_with_ibus();
  return 0;
}
