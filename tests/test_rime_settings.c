#include "qiwo_webdav_config.h"
#include "rime_settings.h"

#include <glib.h>
#include <rime_api.h>
#include <string.h>

RimeApi *rime_api = NULL;

static gchar *test_config_home;
static gboolean stub_has_rime_interval;
static int stub_rime_interval_minutes;

static Bool
stub_config_open(const char *config_id, RimeConfig *config)
{
  g_assert_cmpstr(config_id, ==, "ibus_rime");
  g_assert_nonnull(config);
  return True;
}

static Bool
stub_config_get_bool(RimeConfig *config, const char *key, Bool *value)
{
  g_assert_nonnull(config);
  g_assert_nonnull(key);
  g_assert_nonnull(value);
  return False;
}

static const char *
stub_config_get_cstring(RimeConfig *config, const char *key)
{
  g_assert_nonnull(config);
  g_assert_nonnull(key);
  return NULL;
}

static Bool
stub_config_get_int(RimeConfig *config, const char *key, int *value)
{
  g_assert_nonnull(config);
  g_assert_nonnull(key);
  g_assert_nonnull(value);
  if (g_strcmp0(key, "sync/auto_sync_interval_minutes") != 0 ||
      !stub_has_rime_interval) {
    return False;
  }
  *value = stub_rime_interval_minutes;
  return True;
}

static void
stub_config_close(RimeConfig *config)
{
  g_assert_nonnull(config);
}

static void
setup_test(void)
{
  static RimeApi api = {
    stub_config_open,
    stub_config_get_bool,
    stub_config_get_cstring,
    stub_config_get_int,
    stub_config_close,
  };
  g_autoptr(GError) error = NULL;

  rime_api = &api;
  stub_has_rime_interval = FALSE;
  stub_rime_interval_minutes = 0;
  test_config_home = g_dir_make_tmp("qiwo-rime-settings-XXXXXX", &error);
  g_assert_no_error(error);
  g_assert_nonnull(test_config_home);

  g_setenv("XDG_CONFIG_HOME", test_config_home, TRUE);
  g_unsetenv(QIWO_WEBDAV_ENV_AUTO_SYNC_INTERVAL);
  qiwo_webdav_config_set_secret_service_available_for_tests(FALSE);
}

static void
teardown_test(void)
{
  qiwo_webdav_config_reset_secret_service_available_for_tests();
  g_unsetenv(QIWO_WEBDAV_ENV_AUTO_SYNC_INTERVAL);
  if (test_config_home) {
    g_spawn_command_line_sync(
        g_strdup_printf("rm -rf '%s'", test_config_home), NULL, NULL, NULL, NULL);
    g_clear_pointer(&test_config_home, g_free);
  }
}

static void
save_webdav_interval(guint minutes)
{
  QiwoWebDavSettings settings;
  qiwo_webdav_settings_init(&settings);
  settings.auto_sync_interval_minutes = minutes;

  g_autoptr(GError) error = NULL;
  g_assert_true(qiwo_webdav_config_save(&settings, &error));
  g_assert_no_error(error);

  qiwo_webdav_settings_clear(&settings);
}

static void
test_saved_webdav_interval_is_fallback(void)
{
  setup_test();
  save_webdav_interval(12);

  ibus_rime_load_settings();

  g_assert_cmpuint(g_ibus_rime_settings.auto_sync_interval_seconds, ==, 12 * 60);
  teardown_test();
}

static void
test_rime_interval_wins_over_saved_webdav_interval(void)
{
  setup_test();
  save_webdav_interval(12);
  stub_has_rime_interval = TRUE;
  stub_rime_interval_minutes = 20;

  ibus_rime_load_settings();

  g_assert_cmpuint(g_ibus_rime_settings.auto_sync_interval_seconds, ==, 20 * 60);
  teardown_test();
}

static void
test_env_webdav_interval_wins_over_rime_interval(void)
{
  setup_test();
  save_webdav_interval(12);
  stub_has_rime_interval = TRUE;
  stub_rime_interval_minutes = 20;
  g_setenv(QIWO_WEBDAV_ENV_AUTO_SYNC_INTERVAL, "7", TRUE);

  ibus_rime_load_settings();

  g_assert_cmpuint(g_ibus_rime_settings.auto_sync_interval_seconds, ==, 7 * 60);
  teardown_test();
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/qiwo/rime-settings/saved-webdav-interval-is-fallback",
                  test_saved_webdav_interval_is_fallback);
  g_test_add_func("/qiwo/rime-settings/rime-interval-wins-over-saved-webdav-interval",
                  test_rime_interval_wins_over_saved_webdav_interval);
  g_test_add_func("/qiwo/rime-settings/env-webdav-interval-wins-over-rime-interval",
                  test_env_webdav_interval_wins_over_rime_interval);

  return g_test_run();
}
