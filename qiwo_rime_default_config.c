#include "qiwo_rime_default_config.h"

#include <errno.h>
#include <glib/gstdio.h>

#define QIWO_DEFAULT_CUSTOM_YAML "default.custom.yaml"

static const gchar qiwo_default_custom_yaml_content[] =
    "patch:\n"
    "  schema_list:\n"
    "    - schema: rime_frost\n"
    "  switcher/hotkeys/@next: F4\n"
    "  switcher/save_options/@next: auto_commit_spacing\n";

static const gchar qiwo_schema_custom_yaml_content[] =
    "patch:\n"
    "  switches/@next:\n"
    "    name: auto_commit_spacing\n"
    "    states: [ 关闭中英数字自动空格, 开启中英数字自动空格 ]\n";

static const gchar *qiwo_frost_schema_custom_files[] = {
    "rime_frost.custom.yaml",
    "rime_frost_double_pinyin.custom.yaml",
    "rime_frost_double_pinyin_mspy.custom.yaml",
    "rime_frost_double_pinyin_sogou.custom.yaml",
    "rime_frost_double_pinyin_flypy.custom.yaml",
    "rime_frost_double_pinyin_abc.custom.yaml",
    "rime_frost_double_pinyin_ziguang.custom.yaml",
    "rime_frost_t9.custom.yaml",
    "rime_frost_wubi86.custom.yaml",
    "rime_frost_moqi_single_xh.custom.yaml",
};

static gboolean
is_empty_or_whitespace(const gchar *content)
{
  if (!content) return TRUE;
  for (const gchar *p = content; *p; p++) {
    if (!g_ascii_isspace(*p)) return FALSE;
  }
  return TRUE;
}

static gboolean
ensure_custom_yaml_file(const gchar *user_data_dir,
                        const gchar *file_name,
                        const gchar *content,
                        gboolean *created,
                        GError **error)
{
  g_autofree gchar *path = g_build_filename(user_data_dir, file_name, NULL);
  if (g_file_test(path, G_FILE_TEST_EXISTS)) {
    g_autofree gchar *existing = NULL;
    if (!g_file_get_contents(path, &existing, NULL, error)) {
      return FALSE;
    }
    if (!is_empty_or_whitespace(existing)) {
      return TRUE;
    }
  }

  if (!g_file_set_contents(path, content, -1, error)) {
    return FALSE;
  }
  if (created) *created = TRUE;
  return TRUE;
}

gboolean
qiwo_rime_default_config_ensure(const gchar *user_data_dir,
                                gboolean *created,
                                GError **error)
{
  g_return_val_if_fail(user_data_dir != NULL, FALSE);

  if (created) *created = FALSE;

  if (g_mkdir_with_parents(user_data_dir, 0700) != 0) {
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                "Unable to create Rime user data directory: %s", user_data_dir);
    return FALSE;
  }

  if (!ensure_custom_yaml_file(user_data_dir,
                               QIWO_DEFAULT_CUSTOM_YAML,
                               qiwo_default_custom_yaml_content,
                               created,
                               error)) {
    return FALSE;
  }

  for (gsize i = 0; i < G_N_ELEMENTS(qiwo_frost_schema_custom_files); i++) {
    if (!ensure_custom_yaml_file(user_data_dir,
                                 qiwo_frost_schema_custom_files[i],
                                 qiwo_schema_custom_yaml_content,
                                 created,
                                 error)) {
      return FALSE;
    }
  }

  return TRUE;
}
