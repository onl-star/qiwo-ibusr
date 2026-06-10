#include <gtk/gtk.h>

#include "qiwo_sync_command.h"
#include "qiwo_webdav_config.h"

typedef struct {
  GtkWidget *window;
  GtkWidget *url_entry;
  GtkWidget *remote_path_entry;
  GtkWidget *username_entry;
  GtkWidget *password_entry;
  GtkWidget *device_id_entry;
  GtkWidget *interval_spin;
  GtkWidget *storage_label;
  GtkWidget *override_label;
  GtkWidget *status_label;
  GtkWidget *save_button;
  GtkWidget *test_button;
  GtkWidget *sync_button;
} SettingsWidgets;

static GtkWidget *
add_labeled_entry(GtkGrid *grid,
                  const gchar *label_text,
                  gint row,
                  gboolean password)
{
  GtkWidget *label = gtk_label_new(label_text);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_grid_attach(grid, label, 0, row, 1, 1);

  GtkWidget *entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(entry), !password);
  gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_grid_attach(grid, entry, 1, row, 2, 1);
  return entry;
}

static GtkWidget *
build_window(SettingsWidgets *widgets)
{
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Qiwo WebDAV Settings");
  gtk_window_set_default_size(GTK_WINDOW(window), 560, 400);
  gtk_container_set_border_width(GTK_CONTAINER(window), 16);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
  gtk_container_add(GTK_CONTAINER(window), grid);

  widgets->url_entry = add_labeled_entry(GTK_GRID(grid), "WebDAV Server URL", 0, FALSE);
  widgets->remote_path_entry = add_labeled_entry(GTK_GRID(grid), "Remote Path", 1, FALSE);
  widgets->username_entry = add_labeled_entry(GTK_GRID(grid), "Username", 2, FALSE);
  widgets->password_entry = add_labeled_entry(GTK_GRID(grid), "Password", 3, TRUE);
  widgets->device_id_entry = add_labeled_entry(GTK_GRID(grid), "Device ID", 4, FALSE);

  GtkWidget *interval_label = gtk_label_new("Auto sync interval (minutes)");
  gtk_widget_set_halign(interval_label, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(grid), interval_label, 0, 5, 1, 1);

  widgets->interval_spin = gtk_spin_button_new_with_range(0, 1440, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets->interval_spin), 0);
  gtk_grid_attach(GTK_GRID(grid), widgets->interval_spin, 1, 5, 2, 1);

  widgets->storage_label = gtk_label_new("Password storage: none");
  gtk_label_set_xalign(GTK_LABEL(widgets->storage_label), 0.0);
  gtk_grid_attach(GTK_GRID(grid), widgets->storage_label, 0, 6, 3, 1);

  widgets->override_label = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(widgets->override_label), 0.0);
  gtk_label_set_line_wrap(GTK_LABEL(widgets->override_label), TRUE);
  gtk_widget_set_hexpand(widgets->override_label, TRUE);
  gtk_grid_attach(GTK_GRID(grid), widgets->override_label, 0, 7, 3, 1);

  widgets->status_label = gtk_label_new(
      "Configure WebDAV sync settings. Use the IBus panel WebDAV Sync for user dictionaries.");
  gtk_label_set_xalign(GTK_LABEL(widgets->status_label), 0.0);
  gtk_label_set_line_wrap(GTK_LABEL(widgets->status_label), TRUE);
  gtk_widget_set_hexpand(widgets->status_label, TRUE);
  gtk_grid_attach(GTK_GRID(grid), widgets->status_label, 0, 8, 3, 1);

  GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  gtk_grid_attach(GTK_GRID(grid), button_box, 0, 9, 3, 1);

  widgets->save_button = gtk_button_new_with_label("Save");
  widgets->test_button = gtk_button_new_with_label("Test Connection");
  widgets->sync_button = gtk_button_new_with_label("Sync Config Now");
  gtk_widget_set_tooltip_text(
      widgets->sync_button,
      "Synchronize Rime configuration files. User dictionaries require the IBus panel WebDAV Sync action.");
  gtk_container_add(GTK_CONTAINER(button_box), widgets->save_button);
  gtk_container_add(GTK_CONTAINER(button_box), widgets->test_button);
  gtk_container_add(GTK_CONTAINER(button_box), widgets->sync_button);

  return window;
}

static const gchar *
password_storage_mode_text(QiwoPasswordStorageMode mode)
{
  switch (mode) {
    case QIWO_PASSWORD_STORAGE_SECRET_SERVICE:
      return "Password storage: Secret Service";
    case QIWO_PASSWORD_STORAGE_LOCAL_FILE:
      return "Password storage: protected local file";
    case QIWO_PASSWORD_STORAGE_UNAVAILABLE:
      return "Password storage: unavailable";
    case QIWO_PASSWORD_STORAGE_NONE:
    default:
      return "Password storage: none";
  }
}

static void
load_saved_settings(SettingsWidgets *widgets)
{
  QiwoWebDavSettings settings;
  qiwo_webdav_settings_init(&settings);

  GError *error = NULL;
  if (!qiwo_webdav_config_load(&settings, &error)) {
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       error ? error->message : "Unable to load settings.");
    g_clear_error(&error);
    qiwo_webdav_settings_clear(&settings);
    return;
  }

  if (settings.url) {
    gtk_entry_set_text(GTK_ENTRY(widgets->url_entry), settings.url);
  }
  gtk_entry_set_text(GTK_ENTRY(widgets->remote_path_entry),
                     settings.remote_path ?
                     settings.remote_path : QIWO_WEBDAV_DEFAULT_REMOTE_PATH);
  if (settings.username) {
    gtk_entry_set_text(GTK_ENTRY(widgets->username_entry), settings.username);
  }
  if (settings.password) {
    gtk_entry_set_text(GTK_ENTRY(widgets->password_entry), settings.password);
  }
  if (settings.device_id) {
    gtk_entry_set_text(GTK_ENTRY(widgets->device_id_entry), settings.device_id);
  }
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets->interval_spin),
                            settings.auto_sync_interval_minutes);
  gtk_label_set_text(GTK_LABEL(widgets->storage_label),
                     password_storage_mode_text(settings.password_storage_mode));

  qiwo_webdav_settings_clear(&settings);
}

static void
load_override_notice(SettingsWidgets *widgets)
{
  QiwoEffectiveWebDavSettings settings;
  qiwo_effective_webdav_settings_init(&settings);

  GError *error = NULL;
  if (!qiwo_webdav_config_load_effective(&settings, &error)) {
    gtk_label_set_text(GTK_LABEL(widgets->override_label), "");
    g_clear_error(&error);
    qiwo_effective_webdav_settings_clear(&settings);
    return;
  }

  GString *message = g_string_new("");
  if (settings.url_overridden) g_string_append(message, "remote URL, ");
  if (settings.username_overridden) g_string_append(message, "username, ");
  if (settings.password_overridden) g_string_append(message, "password, ");
  if (settings.device_id_overridden) g_string_append(message, "device ID, ");
  if (settings.auto_sync_interval_overridden) {
    g_string_append(message, "auto-sync interval, ");
  }

  if (message->len > 0) {
    g_string_truncate(message, message->len - 2);
    g_string_prepend(message, "Environment overrides active for: ");
    gtk_label_set_text(GTK_LABEL(widgets->override_label), message->str);
  } else {
    gtk_label_set_text(GTK_LABEL(widgets->override_label), "");
  }

  g_string_free(message, TRUE);
  qiwo_effective_webdav_settings_clear(&settings);
}

static void
save_settings(GtkButton *button, gpointer user_data)
{
  (void)button;
  SettingsWidgets *widgets = user_data;

  QiwoWebDavSettings settings;
  qiwo_webdav_settings_init(&settings);
  settings.url = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->url_entry)));
  settings.remote_path =
      g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->remote_path_entry)));
  settings.username = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->username_entry)));
  settings.password = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->password_entry)));
  settings.device_id = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->device_id_entry)));
  settings.auto_sync_interval_minutes =
      gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets->interval_spin));

  GError *error = NULL;
  if (!qiwo_webdav_config_save(&settings, &error)) {
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       error ? error->message : "Unable to save settings.");
    g_clear_error(&error);
  } else {
    load_saved_settings(widgets);
    load_override_notice(widgets);
    gtk_label_set_text(GTK_LABEL(widgets->status_label), "Settings saved.");
  }

  qiwo_webdav_settings_clear(&settings);
}

static void
collect_effective_settings(SettingsWidgets *widgets,
                           QiwoEffectiveWebDavSettings *settings)
{
  qiwo_effective_webdav_settings_init(settings);
  settings->url = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->url_entry)));
  settings->remote_path =
      g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->remote_path_entry)));
  settings->full_remote_url =
      qiwo_webdav_config_build_full_remote_url(settings->url,
                                               settings->remote_path);
  settings->username = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->username_entry)));
  settings->password = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->password_entry)));
  settings->device_id = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->device_id_entry)));
  settings->auto_sync_interval_minutes =
      gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets->interval_spin));
  settings->password_storage_mode = QIWO_PASSWORD_STORAGE_LOCAL_FILE;

  const gchar *value = g_getenv(QIWO_WEBDAV_ENV_URL);
  if (value && value[0]) {
    g_free(settings->full_remote_url);
    settings->full_remote_url = g_strdup(value);
    settings->url_overridden = TRUE;
  }

  value = g_getenv(QIWO_WEBDAV_ENV_USERNAME);
  if (value && value[0]) {
    g_free(settings->username);
    settings->username = g_strdup(value);
    settings->username_overridden = TRUE;
  }

  value = g_getenv(QIWO_WEBDAV_ENV_PASSWORD);
  if (value && value[0]) {
    g_free(settings->password);
    settings->password = g_strdup(value);
    settings->password_overridden = TRUE;
  }

  value = g_getenv(QIWO_WEBDAV_ENV_DEVICE_ID);
  if (value && value[0]) {
    g_free(settings->device_id);
    settings->device_id = g_strdup(value);
    settings->device_id_overridden = TRUE;
  }
}

static gboolean
validate_current_settings(SettingsWidgets *widgets)
{
  QiwoEffectiveWebDavSettings settings;
  collect_effective_settings(widgets, &settings);

  GError *error = NULL;
  gboolean valid = qiwo_webdav_effective_settings_validate(&settings, &error);
  if (!valid) {
    g_autofree gchar *message = g_strdup_printf(
        "Missing setting: %s",
        error ? error->message : "Required settings are missing.");
    gtk_label_set_text(GTK_LABEL(widgets->status_label), message);
    g_clear_error(&error);
  }

  qiwo_effective_webdav_settings_clear(&settings);
  return valid;
}

static gchar *
get_rime_user_dir(void)
{
  return g_build_filename(g_get_home_dir(), ".config", "ibus", "rime", NULL);
}

static void
set_action_buttons_sensitive(SettingsWidgets *widgets, gboolean sensitive)
{
  gtk_widget_set_sensitive(widgets->save_button, sensitive);
  gtk_widget_set_sensitive(widgets->test_button, sensitive);
  gtk_widget_set_sensitive(widgets->sync_button, sensitive);
}

static void
test_connection(GtkButton *button, gpointer user_data)
{
  (void)button;
  SettingsWidgets *widgets = user_data;
  if (!validate_current_settings(widgets)) {
    return;
  }

  QiwoEffectiveWebDavSettings settings;
  collect_effective_settings(widgets, &settings);
  QiwoSyncCommandResult result;
  qiwo_sync_command_result_init(&result);
  g_autofree gchar *rime_user_dir = get_rime_user_dir();

  set_action_buttons_sensitive(widgets, FALSE);
  GError *error = NULL;
  gboolean ok = qiwo_sync_command_run_sync(
      rime_user_dir, &settings, TRUE, &result, &error);
  if (ok) {
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       "Test connection succeeded.");
  } else if (g_error_matches(error, QIWO_SYNC_COMMAND_ERROR,
                             QIWO_SYNC_COMMAND_ERROR_TOOL_NOT_FOUND)) {
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       "qiwo-rime-sync was not found. Install it and try again.");
    g_clear_error(&error);
  } else {
    const gchar *details =
        result.stderr_text && result.stderr_text[0] ?
        result.stderr_text :
        (error ? error->message : "Test connection failed.");
    g_autofree gchar *message = g_strdup_printf(
        "Test connection failed: %s", details);
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       message);
    g_clear_error(&error);
  }
  set_action_buttons_sensitive(widgets, TRUE);

  qiwo_sync_command_result_clear(&result);
  qiwo_effective_webdav_settings_clear(&settings);
}

static void
sync_now(GtkButton *button, gpointer user_data)
{
  (void)button;
  SettingsWidgets *widgets = user_data;
  if (!validate_current_settings(widgets)) {
    return;
  }

  QiwoEffectiveWebDavSettings settings;
  collect_effective_settings(widgets, &settings);
  QiwoSyncCommandResult result;
  qiwo_sync_command_result_init(&result);
  g_autofree gchar *rime_user_dir = get_rime_user_dir();

  set_action_buttons_sensitive(widgets, FALSE);
  GError *error = NULL;
  gboolean ok = qiwo_sync_command_run_sync(
      rime_user_dir, &settings, FALSE, &result, &error);
  if (ok) {
    if (result.stdout_text && result.stdout_text[0]) {
      g_autofree gchar *summary = g_strdup(result.stdout_text);
      g_strstrip(summary);
      g_autofree gchar *message = g_strdup_printf("Config sync completed: %s", summary);
      gtk_label_set_text(GTK_LABEL(widgets->status_label), message);
    } else {
      gtk_label_set_text(GTK_LABEL(widgets->status_label), "Config sync completed.");
    }
  } else if (g_error_matches(error, QIWO_SYNC_COMMAND_ERROR,
                             QIWO_SYNC_COMMAND_ERROR_TOOL_NOT_FOUND)) {
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       "qiwo-rime-sync was not found. Install it and try again.");
    g_clear_error(&error);
  } else {
    const gchar *details =
        result.stderr_text && result.stderr_text[0] ?
        result.stderr_text :
        (error ? error->message : "Sync failed.");
    g_autofree gchar *message = g_strdup_printf("Sync failed: %s", details);
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       message);
    g_clear_error(&error);
  }
  set_action_buttons_sensitive(widgets, TRUE);

  qiwo_sync_command_result_clear(&result);
  qiwo_effective_webdav_settings_clear(&settings);
}

int
main(int argc, char **argv)
{
  gtk_init(&argc, &argv);

  SettingsWidgets widgets = {0};
  widgets.window = build_window(&widgets);
  load_saved_settings(&widgets);
  load_override_notice(&widgets);
  g_signal_connect(widgets.save_button, "clicked",
                   G_CALLBACK(save_settings), &widgets);
  g_signal_connect(widgets.test_button, "clicked",
                   G_CALLBACK(test_connection), &widgets);
  g_signal_connect(widgets.sync_button, "clicked",
                   G_CALLBACK(sync_now), &widgets);

  gtk_widget_show_all(widgets.window);
  gtk_main();
  return 0;
}
