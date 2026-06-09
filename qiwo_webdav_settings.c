#include <gtk/gtk.h>

#include "qiwo_sync_command.h"
#include "qiwo_webdav_config.h"

typedef struct {
  GtkWidget *window;
  GtkWidget *url_entry;
  GtkWidget *username_entry;
  GtkWidget *password_entry;
  GtkWidget *device_id_entry;
  GtkWidget *interval_spin;
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
  gtk_window_set_default_size(GTK_WINDOW(window), 520, 360);
  gtk_container_set_border_width(GTK_CONTAINER(window), 16);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
  gtk_container_add(GTK_CONTAINER(window), grid);

  widgets->url_entry = add_labeled_entry(GTK_GRID(grid), "WebDAV URL", 0, FALSE);
  widgets->username_entry = add_labeled_entry(GTK_GRID(grid), "Username", 1, FALSE);
  widgets->password_entry = add_labeled_entry(GTK_GRID(grid), "Password", 2, TRUE);
  widgets->device_id_entry = add_labeled_entry(GTK_GRID(grid), "Device ID", 3, FALSE);

  GtkWidget *interval_label = gtk_label_new("Auto sync interval (minutes)");
  gtk_widget_set_halign(interval_label, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(grid), interval_label, 0, 4, 1, 1);

  widgets->interval_spin = gtk_spin_button_new_with_range(0, 1440, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets->interval_spin), 0);
  gtk_grid_attach(GTK_GRID(grid), widgets->interval_spin, 1, 4, 2, 1);

  widgets->status_label = gtk_label_new("Configure WebDAV sync settings.");
  gtk_label_set_xalign(GTK_LABEL(widgets->status_label), 0.0);
  gtk_widget_set_hexpand(widgets->status_label, TRUE);
  gtk_grid_attach(GTK_GRID(grid), widgets->status_label, 0, 5, 3, 1);

  GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  gtk_grid_attach(GTK_GRID(grid), button_box, 0, 6, 3, 1);

  widgets->save_button = gtk_button_new_with_label("Save");
  widgets->test_button = gtk_button_new_with_label("Test Connection");
  widgets->sync_button = gtk_button_new_with_label("Sync Now");
  gtk_container_add(GTK_CONTAINER(button_box), widgets->save_button);
  gtk_container_add(GTK_CONTAINER(button_box), widgets->test_button);
  gtk_container_add(GTK_CONTAINER(button_box), widgets->sync_button);

  return window;
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

  qiwo_webdav_settings_clear(&settings);
}

static void
save_settings(GtkButton *button, gpointer user_data)
{
  (void)button;
  SettingsWidgets *widgets = user_data;

  QiwoWebDavSettings settings;
  qiwo_webdav_settings_init(&settings);
  settings.url = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->url_entry)));
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
  settings->username = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->username_entry)));
  settings->password = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->password_entry)));
  settings->device_id = g_strdup(gtk_entry_get_text(GTK_ENTRY(widgets->device_id_entry)));
  settings->auto_sync_interval_minutes =
      gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets->interval_spin));
  settings->password_storage_mode = QIWO_PASSWORD_STORAGE_LOCAL_FILE;
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
  if (validate_current_settings(widgets)) {
    gtk_label_set_text(GTK_LABEL(widgets->status_label),
                       "Settings are ready to sync.");
  }
}

int
main(int argc, char **argv)
{
  gtk_init(&argc, &argv);

  SettingsWidgets widgets = {0};
  widgets.window = build_window(&widgets);
  load_saved_settings(&widgets);
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
