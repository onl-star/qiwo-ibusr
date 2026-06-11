#include "qiwo_rime_default_config.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

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

typedef struct {
  const gchar *needle;
  const gchar *entry;
} QiwoYamlPatchEntry;

static const QiwoYamlPatchEntry qiwo_default_patch_entries[] = {
    {"switcher/hotkeys/@next: F4", "  switcher/hotkeys/@next: F4"},
    {"switcher/save_options/@next: auto_commit_spacing",
     "  switcher/save_options/@next: auto_commit_spacing"},
};

static const QiwoYamlPatchEntry qiwo_schema_patch_entries[] = {
    {"auto_commit_spacing",
     "  switches/@next:\n"
     "    name: auto_commit_spacing\n"
     "    states: [ 关闭中英数字自动空格, 开启中英数字自动空格 ]"},
};

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

static gchar *
collect_missing_patch_entries(const gchar *content,
                              const QiwoYamlPatchEntry *entries,
                              gsize n_entries)
{
  GString *block = g_string_new(NULL);

  for (gsize i = 0; i < n_entries; i++) {
    if (strstr(content, entries[i].needle)) {
      continue;
    }

    g_string_append(block, entries[i].entry);
    if (!g_str_has_suffix(entries[i].entry, "\n")) {
      g_string_append_c(block, '\n');
    }
  }

  if (block->len == 0) {
    g_string_free(block, TRUE);
    return NULL;
  }
  return g_string_free(block, FALSE);
}

static gchar *
append_yaml_patch_entries(const gchar *content, const gchar *entries_block)
{
  gchar **lines = g_strsplit(content, "\n", -1);
  gint patch_index = -1;
  gint insertion_index = -1;

  for (gint i = 0; lines[i] != NULL; i++) {
    if (g_strcmp0(lines[i], "patch:") == 0) {
      patch_index = i;
      break;
    }
  }

  if (patch_index < 0) {
    GString *output = g_string_new(content);
    if (!g_str_has_suffix(content, "\n")) {
      g_string_append_c(output, '\n');
    }
    if (output->len > 0) {
      g_string_append_c(output, '\n');
    }
    g_string_append(output, "patch:\n");
    g_string_append(output, entries_block);
    g_strfreev(lines);
    return g_string_free(output, FALSE);
  }

  for (gint i = patch_index + 1; lines[i] != NULL; i++) {
    if (lines[i][0] != '\0' &&
        lines[i][0] != ' ' &&
        lines[i][0] != '\t' &&
        lines[i][0] != '#') {
      insertion_index = i;
      break;
    }
  }

  if (insertion_index < 0) {
    insertion_index = g_strv_length(lines);
    if (insertion_index > 0 && lines[insertion_index - 1][0] == '\0') {
      insertion_index--;
    }
  }

  GString *output = g_string_new(NULL);
  for (gint i = 0; lines[i] != NULL; i++) {
    if (i == insertion_index) {
      g_string_append(output, entries_block);
    }
    if (lines[i + 1] == NULL && lines[i][0] == '\0') {
      continue;
    }
    g_string_append(output, lines[i]);
    g_string_append_c(output, '\n');
  }

  if (lines[insertion_index] == NULL) {
    g_string_append(output, entries_block);
  }

  g_strfreev(lines);
  return g_string_free(output, FALSE);
}

static gboolean
ensure_custom_yaml_file(const gchar *user_data_dir,
                        const gchar *file_name,
                        const gchar *content,
                        const QiwoYamlPatchEntry *patch_entries,
                        gsize n_patch_entries,
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
      g_autofree gchar *missing_entries =
          collect_missing_patch_entries(existing, patch_entries, n_patch_entries);
      if (missing_entries) {
        g_autofree gchar *updated =
            append_yaml_patch_entries(existing, missing_entries);
        if (!g_file_set_contents(path, updated, -1, error)) {
          return FALSE;
        }
        if (created) *created = TRUE;
      }
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
                               qiwo_default_patch_entries,
                               G_N_ELEMENTS(qiwo_default_patch_entries),
                               created,
                               error)) {
    return FALSE;
  }

  for (gsize i = 0; i < G_N_ELEMENTS(qiwo_frost_schema_custom_files); i++) {
    if (!ensure_custom_yaml_file(user_data_dir,
                                 qiwo_frost_schema_custom_files[i],
                                 qiwo_schema_custom_yaml_content,
                                 qiwo_schema_patch_entries,
                                 G_N_ELEMENTS(qiwo_schema_patch_entries),
                                 created,
                                 error)) {
      return FALSE;
    }
  }

  return TRUE;
}
