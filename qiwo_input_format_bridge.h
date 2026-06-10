#ifndef __QIWO_INPUT_FORMAT_BRIDGE_H__
#define __QIWO_INPUT_FORMAT_BRIDGE_H__

#include <glib.h>

gchar *
qiwo_input_format_bridge_format_commit_text(const gchar *commit_text,
                                            const gchar *before_cursor,
                                            const gchar *after_cursor,
                                            gboolean auto_spacing_enabled);

#endif
