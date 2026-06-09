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
#include "qiwo_sync_command.h"
#include "qiwo_webdav_config.h"

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

void ibus_rime_sync_user_data(void) {
  char user_data_dir[PATH_MAX];
  get_ibus_rime_user_data_dir(user_data_dir);

  QiwoEffectiveWebDavSettings settings;
  qiwo_effective_webdav_settings_init(&settings);
  GError* error = NULL;
  if (!qiwo_webdav_config_load_effective(&settings, &error)) {
    g_warning("Qiwo WebDAV config load failed: %s",
              error ? error->message : "unknown");
    show_message(_("Qiwo WebDAV sync failed"),
                 error ? error->message : _("Unable to load settings."));
    g_clear_error(&error);
    qiwo_effective_webdav_settings_clear(&settings);
    return;
  }
  if (!settings.device_id || !settings.device_id[0]) {
    g_free(settings.device_id);
    settings.device_id = g_strdup(g_get_host_name());
  }

  // Ensure installation.yaml sync config
  qiwo_ensure_installation_yaml(user_data_dir, settings.device_id);

  if (rime_api) {
    rime_api->sync_user_data();
  }

  QiwoSyncCommandResult result;
  qiwo_sync_command_result_init(&result);
  gboolean ok = qiwo_sync_command_run_sync(
      user_data_dir, &settings, FALSE, &result, &error);

  if (!ok) {
    const gchar* details = error ? error->message :
        (result.stderr_text ? result.stderr_text : _("Unknown error."));
    g_warning("Qiwo WebDAV sync failed: %s", details);
    show_message(_("Qiwo WebDAV sync failed"), details);
  } else if (rime_api) {
    rime_api->sync_user_data();
    show_message(_("Qiwo WebDAV sync complete"),
                 result.stdout_text && result.stdout_text[0] ?
                 result.stdout_text : _("Sync completed."));
  }

  g_clear_error(&error);
  qiwo_sync_command_result_clear(&result);
  qiwo_effective_webdav_settings_clear(&settings);
}

int main(gint argc, gchar** argv) {
  signal(SIGTERM, sigterm_cb);
  signal(SIGINT, sigterm_cb);

  rime_api = rime_get_api();
  rime_with_ibus();
  return 0;
}
