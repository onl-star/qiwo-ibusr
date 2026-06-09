#ifndef QIWO_SYNC_COMMAND_H_
#define QIWO_SYNC_COMMAND_H_

#include <glib.h>

#include "qiwo_webdav_config.h"

G_BEGIN_DECLS

typedef enum {
  QIWO_SYNC_COMMAND_ERROR_TOOL_NOT_FOUND,
  QIWO_SYNC_COMMAND_ERROR_INVALID_SETTINGS,
  QIWO_SYNC_COMMAND_ERROR_SPAWN_FAILED,
  QIWO_SYNC_COMMAND_ERROR_EXIT_FAILED
} QiwoSyncCommandError;

#define QIWO_SYNC_COMMAND_ERROR (qiwo_sync_command_error_quark())

typedef struct {
  gint exit_status;
  gchar *stdout_text;
  gchar *stderr_text;
} QiwoSyncCommandResult;

GQuark qiwo_sync_command_error_quark(void);

void qiwo_sync_command_result_init(QiwoSyncCommandResult *result);
void qiwo_sync_command_result_clear(QiwoSyncCommandResult *result);

gchar *qiwo_sync_command_find_tool(void);
gchar **qiwo_sync_command_build_argv(const gchar *tool_path,
                                     const gchar *rime_user_dir,
                                     const QiwoEffectiveWebDavSettings *settings,
                                     gboolean dry_run);
gboolean qiwo_sync_command_run_sync(const gchar *rime_user_dir,
                                    const QiwoEffectiveWebDavSettings *settings,
                                    gboolean dry_run,
                                    QiwoSyncCommandResult *result,
                                    GError **error);

void qiwo_sync_command_set_tool_path_for_tests(const gchar *tool_path);
void qiwo_sync_command_reset_tool_path_for_tests(void);

G_END_DECLS

#endif  // QIWO_SYNC_COMMAND_H_
