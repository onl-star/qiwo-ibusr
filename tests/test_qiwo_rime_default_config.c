#include "qiwo_rime_default_config.h"

#include <glib.h>
#include <string.h>

static gchar *test_rime_dir;

static void
setup_rime_dir(void)
{
  g_autoptr(GError) error = NULL;
  test_rime_dir = g_dir_make_tmp("qiwo-rime-default-XXXXXX", &error);
  g_assert_no_error(error);
  g_assert_nonnull(test_rime_dir);
}

static void
teardown_rime_dir(void)
{
  if (test_rime_dir) {
    g_spawn_command_line_sync(
        g_strdup_printf("rm -rf '%s'", test_rime_dir), NULL, NULL, NULL, NULL);
    g_clear_pointer(&test_rime_dir, g_free);
  }
}

static gchar *
default_custom_path(void)
{
  return g_build_filename(test_rime_dir, "default.custom.yaml", NULL);
}

static gchar *
frost_custom_path(void)
{
  return g_build_filename(test_rime_dir, "rime_frost.custom.yaml", NULL);
}

static void
test_creates_rime_frost_default(void)
{
  setup_rime_dir();

  gboolean created = FALSE;
  g_autoptr(GError) error = NULL;
  g_assert_true(qiwo_rime_default_config_ensure(test_rime_dir, &created, &error));
  g_assert_no_error(error);
  g_assert_true(created);

  g_autofree gchar *path = default_custom_path();
  g_autofree gchar *content = NULL;
  g_assert_true(g_file_get_contents(path, &content, NULL, &error));
  g_assert_no_error(error);
  g_assert_nonnull(strstr(content, "schema: rime_frost"));
  g_assert_nonnull(strstr(content, "switcher/hotkeys/@next: F4"));
  g_assert_nonnull(strstr(content, "switcher/save_options/@next: auto_commit_spacing"));

  g_autofree gchar *schema_custom_path = frost_custom_path();
  g_clear_pointer(&content, g_free);
  g_assert_true(g_file_get_contents(schema_custom_path, &content, NULL, &error));
  g_assert_no_error(error);
  g_assert_nonnull(strstr(content, "switches/@next"));
  g_assert_nonnull(strstr(content, "auto_commit_spacing"));
  g_assert_nonnull(strstr(content, "关闭中英数字自动空格"));
  g_assert_nonnull(strstr(content, "开启中英数字自动空格"));

  teardown_rime_dir();
}

static void
test_merges_existing_nonempty_custom_files(void)
{
  setup_rime_dir();

  g_autofree gchar *path = default_custom_path();
  const gchar *existing = "patch:\n  schema_list:\n    - schema: luna_pinyin\n";
  g_autoptr(GError) error = NULL;
  g_assert_true(g_file_set_contents(path, existing, -1, &error));
  g_assert_no_error(error);

  g_autofree gchar *schema_custom_path = frost_custom_path();
  const gchar *existing_schema =
      "patch:\n  translator/dictionary: rime_frost\n";
  g_assert_true(g_file_set_contents(schema_custom_path, existing_schema, -1, &error));
  g_assert_no_error(error);

  gboolean created = TRUE;
  g_assert_true(qiwo_rime_default_config_ensure(test_rime_dir, &created, &error));
  g_assert_no_error(error);
  g_assert_true(created);

  g_autofree gchar *content = NULL;
  g_assert_true(g_file_get_contents(path, &content, NULL, &error));
  g_assert_no_error(error);
  g_assert_nonnull(strstr(content, "schema: luna_pinyin"));
  g_assert_null(strstr(content, "schema: rime_frost"));
  g_assert_nonnull(strstr(content, "switcher/hotkeys/@next: F4"));
  g_assert_nonnull(strstr(content, "switcher/save_options/@next: auto_commit_spacing"));

  g_clear_pointer(&content, g_free);
  g_assert_true(g_file_get_contents(schema_custom_path, &content, NULL, &error));
  g_assert_no_error(error);
  g_assert_nonnull(strstr(content, "translator/dictionary: rime_frost"));
  g_assert_nonnull(strstr(content, "switches/@next"));
  g_assert_nonnull(strstr(content, "auto_commit_spacing"));

  teardown_rime_dir();
}

static void
test_replaces_empty_default(void)
{
  setup_rime_dir();

  g_autofree gchar *path = default_custom_path();
  g_autoptr(GError) error = NULL;
  g_assert_true(g_file_set_contents(path, "", -1, &error));
  g_assert_no_error(error);

  gboolean created = FALSE;
  g_assert_true(qiwo_rime_default_config_ensure(test_rime_dir, &created, &error));
  g_assert_no_error(error);
  g_assert_true(created);

  g_autofree gchar *content = NULL;
  g_assert_true(g_file_get_contents(path, &content, NULL, &error));
  g_assert_no_error(error);
  g_assert_nonnull(strstr(content, "schema: rime_frost"));

  teardown_rime_dir();
}

int
main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/qiwo/rime-default-config/creates-rime-frost-default",
                  test_creates_rime_frost_default);
  g_test_add_func("/qiwo/rime-default-config/merges-existing-nonempty-custom-files",
                  test_merges_existing_nonempty_custom_files);
  g_test_add_func("/qiwo/rime-default-config/replaces-empty-default",
                  test_replaces_empty_default);

  return g_test_run();
}
