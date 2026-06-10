#include "qiwo_input_format_bridge.h"

#include "qiwo_input_format.h"

gchar *
qiwo_input_format_bridge_format_commit_text(const gchar *commit_text,
                                            const gchar *before_cursor,
                                            const gchar *after_cursor,
                                            gboolean auto_spacing_enabled)
{
  if (!commit_text) {
    return g_strdup("");
  }

  QiwoInputFormatOptions options = {
    .auto_spacing_enabled = auto_spacing_enabled ? true : false,
  };
  char *formatted = qiwo_input_format_commit_text(
      commit_text, before_cursor, after_cursor, options);
  if (!formatted) {
    return g_strdup(commit_text);
  }

  gchar *result = g_strdup(formatted);
  qiwo_input_format_free_string(formatted);
  return result;
}
