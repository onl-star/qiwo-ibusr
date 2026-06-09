#include "qiwo_sync_command.h"

#include <glib.h>
#include <glib/gstdio.h>

static QiwoEffectiveWebDavSettings
make_effective_settings(void)
{
  QiwoEffectiveWebDavSettings settings;
  qiwo_effective_webdav_settings_init(&settings);
  settings.url = g_strdup("https://dav.example.com/qiwo path?x=1&y=2");
  settings.username = g_strdup("user name");
  settings.password = g_strdup("secret value");
  settings.device_id = g_strdup("linux main");
  return settings;
}

static void
test_build_argv_preserves_special_values(void)
{
  QiwoEffectiveWebDavSettings settings = make_effective_settings();
  g_auto(GStrv) argv = qiwo_sync_command_build_argv(
      "/usr/local/bin/qiwo-rime-sync",
      "/home/me/.config/ibus/rime",
      &settings,
      FALSE);

  g_assert_nonnull(argv);
  g_assert_cmpstr(argv[0], ==, "/usr/local/bin/qiwo-rime-sync");
  g_assert_cmpstr(argv[1], ==, "sync");
  g_assert_cmpstr(argv[2], ==, "--frontend");
  g_assert_cmpstr(argv[3], ==, "ibus-rime");
  g_assert_cmpstr(argv[4], ==, "--rime-user-dir");
  g_assert_cmpstr(argv[5], ==, "/home/me/.config/ibus/rime");
  g_assert_cmpstr(argv[6], ==, "--remote-url");
  g_assert_cmpstr(argv[7], ==, "https://dav.example.com/qiwo path?x=1&y=2");
  g_assert_cmpstr(argv[8], ==, "--username");
  g_assert_cmpstr(argv[9], ==, "user name");
  g_assert_cmpstr(argv[10], ==, "--password-env");
  g_assert_cmpstr(argv[11], ==, "QIWO_WEBDAV_PASSWORD");
  g_assert_cmpstr(argv[12], ==, "--device-id");
  g_assert_cmpstr(argv[13], ==, "linux main");
  g_assert_null(argv[14]);

  qiwo_effective_webdav_settings_clear(&settings);
}

static void
test_build_argv_adds_dry_run(void)
{
  QiwoEffectiveWebDavSettings settings = make_effective_settings();
  g_auto(GStrv) argv = qiwo_sync_command_build_argv(
      "/usr/local/bin/qiwo-rime-sync",
      "/home/me/.config/ibus/rime",
      &settings,
      TRUE);

  g_assert_nonnull(argv);
  g_assert_cmpstr(argv[14], ==, "--dry-run");
  g_assert_null(argv[15]);

  qiwo_effective_webdav_settings_clear(&settings);
}

static void
test_find_tool_uses_executable_override(void)
{
  g_autofree gchar *tmp_dir = g_dir_make_tmp("qiwo-sync-command-XXXXXX", NULL);
  g_autofree gchar *tool = g_build_filename(tmp_dir, "qiwo-rime-sync", NULL);
  g_assert_true(g_file_set_contents(tool, "#!/bin/sh\nexit 0\n", -1, NULL));
  g_assert_cmpint(g_chmod(tool, 0700), ==, 0);

  qiwo_sync_command_set_tool_path_for_tests(tool);
  g_autofree gchar *found = qiwo_sync_command_find_tool();
  g_assert_cmpstr(found, ==, tool);
  qiwo_sync_command_reset_tool_path_for_tests();
}

static void
test_run_reports_missing_tool(void)
{
  QiwoEffectiveWebDavSettings settings = make_effective_settings();
  QiwoSyncCommandResult result;
  qiwo_sync_command_result_init(&result);

  qiwo_sync_command_set_tool_path_for_tests("/not/a/real/qiwo-rime-sync");
  g_autoptr(GError) error = NULL;
  g_assert_false(qiwo_sync_command_run_sync(
      "/home/me/.config/ibus/rime",
      &settings,
      TRUE,
      &result,
      &error));
  g_assert_error(error, QIWO_SYNC_COMMAND_ERROR, QIWO_SYNC_COMMAND_ERROR_TOOL_NOT_FOUND);

  qiwo_sync_command_reset_tool_path_for_tests();
  qiwo_sync_command_result_clear(&result);
  qiwo_effective_webdav_settings_clear(&settings);
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/qiwo/sync-command/build-argv-preserves-special-values",
                  test_build_argv_preserves_special_values);
  g_test_add_func("/qiwo/sync-command/build-argv-adds-dry-run",
                  test_build_argv_adds_dry_run);
  g_test_add_func("/qiwo/sync-command/find-tool-uses-executable-override",
                  test_find_tool_uses_executable_override);
  g_test_add_func("/qiwo/sync-command/run-reports-missing-tool",
                  test_run_reports_missing_tool);

  return g_test_run();
}
