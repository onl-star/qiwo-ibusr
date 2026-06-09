#ifndef QIWO_WEBDAV_CONFIG_H_
#define QIWO_WEBDAV_CONFIG_H_

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  QIWO_PASSWORD_STORAGE_NONE = 0,
  QIWO_PASSWORD_STORAGE_SECRET_SERVICE,
  QIWO_PASSWORD_STORAGE_LOCAL_FILE,
  QIWO_PASSWORD_STORAGE_UNAVAILABLE
} QiwoPasswordStorageMode;

typedef enum {
  QIWO_WEBDAV_CONFIG_ERROR_MISSING_URL,
  QIWO_WEBDAV_CONFIG_ERROR_MISSING_USERNAME,
  QIWO_WEBDAV_CONFIG_ERROR_MISSING_PASSWORD,
  QIWO_WEBDAV_CONFIG_ERROR_MISSING_DEVICE_ID,
  QIWO_WEBDAV_CONFIG_ERROR_SAVE_FAILED,
  QIWO_WEBDAV_CONFIG_ERROR_LOAD_FAILED
} QiwoWebDavConfigError;

#define QIWO_WEBDAV_CONFIG_ERROR (qiwo_webdav_config_error_quark())

#define QIWO_WEBDAV_ENV_URL "QIWO_WEBDAV_URL"
#define QIWO_WEBDAV_ENV_USERNAME "QIWO_WEBDAV_USERNAME"
#define QIWO_WEBDAV_ENV_PASSWORD "QIWO_WEBDAV_PASSWORD"
#define QIWO_WEBDAV_ENV_DEVICE_ID "QIWO_DEVICE_ID"
#define QIWO_WEBDAV_ENV_AUTO_SYNC_INTERVAL "QIWO_AUTO_SYNC_INTERVAL_MINUTES"

typedef struct {
  gchar *url;
  gchar *username;
  gchar *password;
  gchar *device_id;
  guint auto_sync_interval_minutes;
  QiwoPasswordStorageMode password_storage_mode;
} QiwoWebDavSettings;

typedef struct {
  gchar *url;
  gchar *username;
  gchar *password;
  gchar *device_id;
  guint auto_sync_interval_minutes;
  QiwoPasswordStorageMode password_storage_mode;
  gboolean url_overridden;
  gboolean username_overridden;
  gboolean password_overridden;
  gboolean device_id_overridden;
  gboolean auto_sync_interval_overridden;
} QiwoEffectiveWebDavSettings;

GQuark qiwo_webdav_config_error_quark(void);

void qiwo_webdav_settings_init(QiwoWebDavSettings *settings);
void qiwo_webdav_settings_clear(QiwoWebDavSettings *settings);

void qiwo_effective_webdav_settings_init(QiwoEffectiveWebDavSettings *settings);
void qiwo_effective_webdav_settings_clear(QiwoEffectiveWebDavSettings *settings);

gchar *qiwo_webdav_config_get_file_path(void);
gboolean qiwo_webdav_config_load(QiwoWebDavSettings *settings, GError **error);
gboolean qiwo_webdav_config_save(const QiwoWebDavSettings *settings, GError **error);
gboolean qiwo_webdav_config_delete_password(GError **error);
gboolean qiwo_webdav_config_load_effective(QiwoEffectiveWebDavSettings *settings,
                                           GError **error);
guint qiwo_webdav_config_get_effective_auto_sync_interval_minutes(
    gboolean *overridden,
    GError **error);
gboolean qiwo_webdav_effective_settings_validate(
    const QiwoEffectiveWebDavSettings *settings,
    GError **error);

void qiwo_webdav_config_set_secret_service_available_for_tests(gboolean available);
void qiwo_webdav_config_reset_secret_service_available_for_tests(void);

G_END_DECLS

#endif  // QIWO_WEBDAV_CONFIG_H_
