#include <gtk/gtk.h>

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

int
main(int argc, char **argv)
{
  gtk_init(&argc, &argv);

  SettingsWidgets widgets = {0};
  widgets.window = build_window(&widgets);

  gtk_widget_show_all(widgets.window);
  gtk_main();
  return 0;
}
