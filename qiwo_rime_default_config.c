#include "qiwo_rime_default_config.h"

#include <errno.h>
#include <glib/gstdio.h>

#define QIWO_DEFAULT_CUSTOM_YAML "default.custom.yaml"

static const gchar qiwo_default_custom_yaml_content[] =
    "patch:\n"
    "  schema_list:\n"
    "    - schema: rime_frost\n";

static gboolean
is_empty_or_whitespace(const gchar *content)
{
  if (!content) return TRUE;
  for (const gchar *p = content; *p; p++) {
    if (!g_ascii_isspace(*p)) return FALSE;
  }
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

  g_autofree gchar *path =
      g_build_filename(user_data_dir, QIWO_DEFAULT_CUSTOM_YAML, NULL);
  if (g_file_test(path, G_FILE_TEST_EXISTS)) {
    g_autofree gchar *content = NULL;
    if (!g_file_get_contents(path, &content, NULL, error)) {
      return FALSE;
    }
    if (!is_empty_or_whitespace(content)) {
      return TRUE;
    }
  }

  if (!g_file_set_contents(path, qiwo_default_custom_yaml_content, -1, error)) {
    return FALSE;
  }
  if (created) *created = TRUE;
  return TRUE;
}
