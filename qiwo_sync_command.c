#include "qiwo_sync_command.h"

#include <gio/gio.h>
#include <string.h>

static gchar *tool_path_for_tests = NULL;

GQuark
qiwo_sync_command_error_quark(void)
{
  return g_quark_from_static_string("qiwo-sync-command-error");
}

void
qiwo_sync_command_result_init(QiwoSyncCommandResult *result)
{
  g_return_if_fail(result != NULL);
  memset(result, 0, sizeof(*result));
  result->exit_status = -1;
}

void
qiwo_sync_command_result_clear(QiwoSyncCommandResult *result)
{
  if (!result) return;
  result->exit_status = -1;
  g_clear_pointer(&result->stdout_text, g_free);
  g_clear_pointer(&result->stderr_text, g_free);
}

void
qiwo_sync_command_set_tool_path_for_tests(const gchar *tool_path)
{
  g_free(tool_path_for_tests);
  tool_path_for_tests = g_strdup(tool_path);
}

void
qiwo_sync_command_reset_tool_path_for_tests(void)
{
  g_clear_pointer(&tool_path_for_tests, g_free);
}

gchar *
qiwo_sync_command_find_tool(void)
{
  if (tool_path_for_tests) {
    return g_file_test(tool_path_for_tests, G_FILE_TEST_IS_EXECUTABLE) ?
        g_strdup(tool_path_for_tests) : NULL;
  }

  static const gchar *paths[] = {
#ifdef QIWO_SYNC_DIR
    QIWO_SYNC_DIR "/qiwo-rime-sync",
#endif
    "/usr/bin/qiwo-rime-sync",
    "/usr/local/bin/qiwo-rime-sync",
    "/usr/share/qiwo/qiwo-rime-sync",
    "/usr/local/share/qiwo/qiwo-rime-sync",
    NULL
  };

  for (guint i = 0; paths[i]; i++) {
    if (g_file_test(paths[i], G_FILE_TEST_IS_EXECUTABLE)) {
      return g_strdup(paths[i]);
    }
  }
  return NULL;
}

gchar **
qiwo_sync_command_build_argv(const gchar *tool_path,
                             const gchar *rime_user_dir,
                             const QiwoEffectiveWebDavSettings *settings,
                             gboolean dry_run)
{
  g_return_val_if_fail(tool_path != NULL, NULL);
  g_return_val_if_fail(rime_user_dir != NULL, NULL);
  g_return_val_if_fail(settings != NULL, NULL);

  GPtrArray *argv = g_ptr_array_new_with_free_func(g_free);
  g_ptr_array_add(argv, g_strdup(tool_path));
  g_ptr_array_add(argv, g_strdup("sync"));
  g_ptr_array_add(argv, g_strdup("--frontend"));
  g_ptr_array_add(argv, g_strdup("ibus-rime"));
  g_ptr_array_add(argv, g_strdup("--rime-user-dir"));
  g_ptr_array_add(argv, g_strdup(rime_user_dir));
  g_ptr_array_add(argv, g_strdup("--remote-url"));
  g_ptr_array_add(argv, g_strdup(settings->url));
  g_ptr_array_add(argv, g_strdup("--username"));
  g_ptr_array_add(argv, g_strdup(settings->username));
  g_ptr_array_add(argv, g_strdup("--password-env"));
  g_ptr_array_add(argv, g_strdup("QIWO_WEBDAV_PASSWORD"));
  g_ptr_array_add(argv, g_strdup("--device-id"));
  g_ptr_array_add(argv, g_strdup(settings->device_id));
  if (dry_run) {
    g_ptr_array_add(argv, g_strdup("--dry-run"));
  }
  g_ptr_array_add(argv, NULL);
  return (gchar **)g_ptr_array_free(argv, FALSE);
}

gboolean
qiwo_sync_command_run_sync(const gchar *rime_user_dir,
                           const QiwoEffectiveWebDavSettings *settings,
                           gboolean dry_run,
                           QiwoSyncCommandResult *result,
                           GError **error)
{
  g_return_val_if_fail(result != NULL, FALSE);

  if (!qiwo_webdav_effective_settings_validate(settings, error)) {
    return FALSE;
  }

  g_autofree gchar *tool = qiwo_sync_command_find_tool();
  if (!tool) {
    g_set_error(error, QIWO_SYNC_COMMAND_ERROR,
                QIWO_SYNC_COMMAND_ERROR_TOOL_NOT_FOUND,
                "qiwo-rime-sync was not found.");
    return FALSE;
  }

  g_auto(GStrv) argv =
      qiwo_sync_command_build_argv(tool, rime_user_dir, settings, dry_run);
  if (!argv) {
    g_set_error(error, QIWO_SYNC_COMMAND_ERROR,
                QIWO_SYNC_COMMAND_ERROR_INVALID_SETTINGS,
                "Unable to build qiwo-rime-sync arguments.");
    return FALSE;
  }

  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new(
      G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
  if (settings->password && settings->password[0]) {
    g_subprocess_launcher_setenv(
        launcher, "QIWO_WEBDAV_PASSWORD", settings->password, TRUE);
  }

  g_autoptr(GSubprocess) process =
      g_subprocess_launcher_spawnv(
          launcher, (const gchar * const *)argv, error);
  if (!process) {
    return FALSE;
  }

  g_autofree gchar *stdout_text = NULL;
  g_autofree gchar *stderr_text = NULL;
  if (!g_subprocess_communicate_utf8(
          process, NULL, NULL, &stdout_text, &stderr_text, error)) {
    return FALSE;
  }

  result->exit_status = g_subprocess_get_exit_status(process);
  result->stdout_text = g_strdup(stdout_text ? stdout_text : "");
  result->stderr_text = g_strdup(stderr_text ? stderr_text : "");

  if (!g_subprocess_get_successful(process)) {
    g_set_error(error, QIWO_SYNC_COMMAND_ERROR,
                QIWO_SYNC_COMMAND_ERROR_EXIT_FAILED,
                "qiwo-rime-sync exited with status %d.", result->exit_status);
    return FALSE;
  }

  return TRUE;
}
