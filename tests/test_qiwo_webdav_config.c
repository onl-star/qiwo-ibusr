#include "qiwo_webdav_config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>

static gchar *test_config_home;

static void
setup_config_home(void)
{
  g_autoptr(GError) error = NULL;

  test_config_home = g_dir_make_tmp("qiwo-webdav-config-XXXXXX", &error);
  g_assert_no_error(error);
  g_assert_nonnull(test_config_home);

  g_setenv("XDG_CONFIG_HOME", test_config_home, TRUE);
  g_unsetenv("QIWO_WEBDAV_URL");
  g_unsetenv("QIWO_WEBDAV_USERNAME");
  g_unsetenv("QIWO_WEBDAV_PASSWORD");
  g_unsetenv("QIWO_DEVICE_ID");
  g_unsetenv("QIWO_AUTO_SYNC_INTERVAL_MINUTES");
  qiwo_webdav_config_set_secret_service_available_for_tests(FALSE);
}

static void
teardown_config_home(void)
{
  qiwo_webdav_config_reset_secret_service_available_for_tests();
  if (test_config_home) {
    g_spawn_command_line_sync(
        g_strdup_printf("rm -rf '%s'", test_config_home), NULL, NULL, NULL, NULL);
    g_clear_pointer(&test_config_home, g_free);
  }
}

static void
test_xdg_config_path(void)
{
  setup_config_home();

  g_autofree gchar *path = qiwo_webdav_config_get_file_path();
  g_autofree gchar *expected =
      g_build_filename(test_config_home, "qiwo", "webdav.conf", NULL);

  g_assert_cmpstr(path, ==, expected);

  teardown_config_home();
}

static void
test_environment_overrides_saved_values(void)
{
  setup_config_home();

  QiwoWebDavSettings saved;
  qiwo_webdav_settings_init(&saved);
  saved.url = g_strdup("https://saved.example.com/dav");
  saved.username = g_strdup("saved-user");
  saved.password = g_strdup("saved-password");
  saved.device_id = g_strdup("saved-device");
  saved.auto_sync_interval_minutes = 30;

  g_autoptr(GError) error = NULL;
  g_assert_true(qiwo_webdav_config_save(&saved, &error));
  g_assert_no_error(error);

  g_setenv("QIWO_WEBDAV_URL", "https://env.example.com/dav", TRUE);
  g_setenv("QIWO_WEBDAV_USERNAME", "env-user", TRUE);
  g_setenv("QIWO_WEBDAV_PASSWORD", "env-password", TRUE);
  g_setenv("QIWO_DEVICE_ID", "env-device", TRUE);
  g_setenv("QIWO_AUTO_SYNC_INTERVAL_MINUTES", "45", TRUE);

  QiwoEffectiveWebDavSettings effective;
  qiwo_effective_webdav_settings_init(&effective);
  g_assert_true(qiwo_webdav_config_load_effective(&effective, &error));
  g_assert_no_error(error);

  g_assert_cmpstr(effective.url, ==, "https://env.example.com/dav");
  g_assert_cmpstr(effective.username, ==, "env-user");
  g_assert_cmpstr(effective.password, ==, "env-password");
  g_assert_cmpstr(effective.device_id, ==, "env-device");
  g_assert_cmpuint(effective.auto_sync_interval_minutes, ==, 45);
  g_assert_true(effective.url_overridden);
  g_assert_true(effective.username_overridden);
  g_assert_true(effective.password_overridden);
  g_assert_true(effective.device_id_overridden);
  g_assert_true(effective.auto_sync_interval_overridden);

  qiwo_effective_webdav_settings_clear(&effective);
  qiwo_webdav_settings_clear(&saved);
  teardown_config_home();
}

static void
test_required_field_validation(void)
{
  QiwoEffectiveWebDavSettings effective;
  qiwo_effective_webdav_settings_init(&effective);
  effective.username = g_strdup("user");
  effective.password = g_strdup("password");
  effective.device_id = g_strdup("linux-main");

  g_autoptr(GError) error = NULL;
  g_assert_false(qiwo_webdav_effective_settings_validate(&effective, &error));
  g_assert_error(error, QIWO_WEBDAV_CONFIG_ERROR, QIWO_WEBDAV_CONFIG_ERROR_MISSING_URL);

  g_clear_error(&error);
  effective.url = g_strdup("https://dav.example.com/qiwo-rime-sync");
  g_assert_true(qiwo_webdav_effective_settings_validate(&effective, &error));
  g_assert_no_error(error);

  qiwo_effective_webdav_settings_clear(&effective);
}

static void
test_fallback_file_mode_is_user_only(void)
{
  setup_config_home();

  QiwoWebDavSettings saved;
  qiwo_webdav_settings_init(&saved);
  saved.url = g_strdup("https://saved.example.com/dav");
  saved.username = g_strdup("saved-user");
  saved.password = g_strdup("saved-password");
  saved.device_id = g_strdup("saved-device");

  g_autoptr(GError) error = NULL;
  g_assert_true(qiwo_webdav_config_save(&saved, &error));
  g_assert_no_error(error);

  g_autofree gchar *path = qiwo_webdav_config_get_file_path();
  struct stat st;
  g_assert_cmpint(g_stat(path, &st), ==, 0);
  g_assert_cmpint(st.st_mode & 0777, ==, 0600);

  QiwoWebDavSettings loaded;
  qiwo_webdav_settings_init(&loaded);
  g_assert_true(qiwo_webdav_config_load(&loaded, &error));
  g_assert_no_error(error);
  g_assert_cmpstr(loaded.password, ==, "saved-password");
  g_assert_cmpint(loaded.password_storage_mode, ==, QIWO_PASSWORD_STORAGE_LOCAL_FILE);

  qiwo_webdav_settings_clear(&loaded);
  qiwo_webdav_settings_clear(&saved);
  teardown_config_home();
}

static void
test_delete_password_removes_local_fallback(void)
{
  setup_config_home();

  QiwoWebDavSettings saved;
  qiwo_webdav_settings_init(&saved);
  saved.url = g_strdup("https://saved.example.com/dav");
  saved.username = g_strdup("saved-user");
  saved.password = g_strdup("saved-password");
  saved.device_id = g_strdup("saved-device");

  g_autoptr(GError) error = NULL;
  g_assert_true(qiwo_webdav_config_save(&saved, &error));
  g_assert_no_error(error);
  g_assert_true(qiwo_webdav_config_delete_password(&error));
  g_assert_no_error(error);

  QiwoWebDavSettings loaded;
  qiwo_webdav_settings_init(&loaded);
  g_assert_true(qiwo_webdav_config_load(&loaded, &error));
  g_assert_no_error(error);
  g_assert_null(loaded.password);
  g_assert_cmpint(loaded.password_storage_mode, ==, QIWO_PASSWORD_STORAGE_NONE);

  qiwo_webdav_settings_clear(&loaded);
  qiwo_webdav_settings_clear(&saved);
  teardown_config_home();
}

static void
test_effective_auto_sync_interval_lookup(void)
{
  setup_config_home();

  QiwoWebDavSettings saved;
  qiwo_webdav_settings_init(&saved);
  saved.url = g_strdup("https://saved.example.com/dav");
  saved.username = g_strdup("saved-user");
  saved.password = g_strdup("saved-password");
  saved.device_id = g_strdup("saved-device");
  saved.auto_sync_interval_minutes = 25;

  g_autoptr(GError) error = NULL;
  g_assert_true(qiwo_webdav_config_save(&saved, &error));
  g_assert_no_error(error);

  gboolean overridden = TRUE;
  g_assert_cmpuint(
      qiwo_webdav_config_get_effective_auto_sync_interval_minutes(&overridden,
                                                                  &error),
      ==, 25);
  g_assert_no_error(error);
  g_assert_false(overridden);

  g_setenv("QIWO_AUTO_SYNC_INTERVAL_MINUTES", "15", TRUE);
  overridden = FALSE;
  g_assert_cmpuint(
      qiwo_webdav_config_get_effective_auto_sync_interval_minutes(&overridden,
                                                                  &error),
      ==, 15);
  g_assert_no_error(error);
  g_assert_true(overridden);

  g_setenv("QIWO_AUTO_SYNC_INTERVAL_MINUTES", "invalid", TRUE);
  overridden = TRUE;
  g_assert_cmpuint(
      qiwo_webdav_config_get_effective_auto_sync_interval_minutes(&overridden,
                                                                  &error),
      ==, 25);
  g_assert_no_error(error);
  g_assert_false(overridden);

  qiwo_webdav_settings_clear(&saved);
  teardown_config_home();
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/qiwo/webdav-config/xdg-config-path", test_xdg_config_path);
  g_test_add_func("/qiwo/webdav-config/environment-overrides-saved-values",
                  test_environment_overrides_saved_values);
  g_test_add_func("/qiwo/webdav-config/required-field-validation",
                  test_required_field_validation);
  g_test_add_func("/qiwo/webdav-config/fallback-file-mode-is-user-only",
                  test_fallback_file_mode_is_user_only);
  g_test_add_func("/qiwo/webdav-config/delete-password-removes-local-fallback",
                  test_delete_password_removes_local_fallback);
  g_test_add_func("/qiwo/webdav-config/effective-auto-sync-interval-lookup",
                  test_effective_auto_sync_interval_lookup);

  return g_test_run();
}
