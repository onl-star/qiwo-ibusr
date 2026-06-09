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

  teardown_rime_dir();
}

static void
test_preserves_existing_nonempty_default(void)
{
  setup_rime_dir();

  g_autofree gchar *path = default_custom_path();
  const gchar *existing = "patch:\n  schema_list:\n    - schema: luna_pinyin\n";
  g_autoptr(GError) error = NULL;
  g_assert_true(g_file_set_contents(path, existing, -1, &error));
  g_assert_no_error(error);

  gboolean created = TRUE;
  g_assert_true(qiwo_rime_default_config_ensure(test_rime_dir, &created, &error));
  g_assert_no_error(error);
  g_assert_false(created);

  g_autofree gchar *content = NULL;
  g_assert_true(g_file_get_contents(path, &content, NULL, &error));
  g_assert_no_error(error);
  g_assert_cmpstr(content, ==, existing);

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
  g_test_add_func("/qiwo/rime-default-config/preserves-existing-nonempty-default",
                  test_preserves_existing_nonempty_default);
  g_test_add_func("/qiwo/rime-default-config/replaces-empty-default",
                  test_replaces_empty_default);

  return g_test_run();
}
