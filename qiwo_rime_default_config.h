#ifndef QIWO_RIME_DEFAULT_CONFIG_H_
#define QIWO_RIME_DEFAULT_CONFIG_H_

#include <glib.h>

G_BEGIN_DECLS

gboolean qiwo_rime_default_config_ensure(const gchar *user_data_dir,
                                         gboolean *created,
                                         GError **error);

G_END_DECLS

#endif  // QIWO_RIME_DEFAULT_CONFIG_H_
