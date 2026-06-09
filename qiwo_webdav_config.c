#include "qiwo_webdav_config.h"

#include <glib/gstdio.h>
#include <string.h>

#ifdef QIWO_WITH_LIBSECRET
#include <libsecret/secret.h>
#endif

#define QIWO_CONFIG_DIR "qiwo"
#define QIWO_CONFIG_FILE "webdav.conf"
#define QIWO_CONFIG_GROUP "webdav"
#define QIWO_SECRET_ATTRIBUTE_SERVICE "service"
#define QIWO_SECRET_ATTRIBUTE_VALUE "qiwo-webdav"

static gboolean secret_service_available_override_set = FALSE;
static gboolean secret_service_available_override = TRUE;

#ifdef QIWO_WITH_LIBSECRET
static const SecretSchema qiwo_webdav_secret_schema = {
  "im.qiwo.webdav",
  SECRET_SCHEMA_NONE,
  {
    { QIWO_SECRET_ATTRIBUTE_SERVICE, SECRET_SCHEMA_ATTRIBUTE_STRING },
    { NULL, 0 },
  }
};
#endif

GQuark
qiwo_webdav_config_error_quark(void)
{
  return g_quark_from_static_string("qiwo-webdav-config-error");
}

void
qiwo_webdav_settings_init(QiwoWebDavSettings *settings)
{
  g_return_if_fail(settings != NULL);
  memset(settings, 0, sizeof(*settings));
  settings->password_storage_mode = QIWO_PASSWORD_STORAGE_NONE;
}

void
qiwo_webdav_settings_clear(QiwoWebDavSettings *settings)
{
  if (!settings) return;
  g_clear_pointer(&settings->url, g_free);
  g_clear_pointer(&settings->username, g_free);
  g_clear_pointer(&settings->password, g_free);
  g_clear_pointer(&settings->device_id, g_free);
  settings->auto_sync_interval_minutes = 0;
  settings->password_storage_mode = QIWO_PASSWORD_STORAGE_NONE;
}

void
qiwo_effective_webdav_settings_init(QiwoEffectiveWebDavSettings *settings)
{
  g_return_if_fail(settings != NULL);
  memset(settings, 0, sizeof(*settings));
  settings->password_storage_mode = QIWO_PASSWORD_STORAGE_NONE;
}

void
qiwo_effective_webdav_settings_clear(QiwoEffectiveWebDavSettings *settings)
{
  if (!settings) return;
  g_clear_pointer(&settings->url, g_free);
  g_clear_pointer(&settings->username, g_free);
  g_clear_pointer(&settings->password, g_free);
  g_clear_pointer(&settings->device_id, g_free);
  settings->auto_sync_interval_minutes = 0;
  settings->password_storage_mode = QIWO_PASSWORD_STORAGE_NONE;
  settings->url_overridden = FALSE;
  settings->username_overridden = FALSE;
  settings->password_overridden = FALSE;
  settings->device_id_overridden = FALSE;
  settings->auto_sync_interval_overridden = FALSE;
}

gchar *
qiwo_webdav_config_get_file_path(void)
{
  const gchar *xdg_config_home = g_getenv("XDG_CONFIG_HOME");
  if (xdg_config_home && xdg_config_home[0]) {
    return g_build_filename(xdg_config_home, QIWO_CONFIG_DIR, QIWO_CONFIG_FILE, NULL);
  }
  return g_build_filename(g_get_user_config_dir(), QIWO_CONFIG_DIR, QIWO_CONFIG_FILE, NULL);
}

static void
set_string_if_present(GKeyFile *key_file,
                      const gchar *key,
                      const gchar *value)
{
  if (value && value[0]) {
    g_key_file_set_string(key_file, QIWO_CONFIG_GROUP, key, value);
  }
}

static gchar *
get_optional_string(GKeyFile *key_file,
                    const gchar *key)
{
  g_autoptr(GError) error = NULL;
  gchar *value = g_key_file_get_string(key_file, QIWO_CONFIG_GROUP, key, &error);
  if (error) {
    g_clear_error(&error);
    return NULL;
  }
  return value;
}

static gboolean
secret_service_allowed(void)
{
  if (secret_service_available_override_set) {
    return secret_service_available_override;
  }
#ifdef QIWO_WITH_LIBSECRET
  return TRUE;
#else
  return FALSE;
#endif
}

static gboolean
try_store_secret_password(const gchar *password)
{
  if (!password || !password[0] || !secret_service_allowed()) {
    return FALSE;
  }
#ifdef QIWO_WITH_LIBSECRET
  g_autoptr(GError) error = NULL;
  return secret_password_store_sync(
      &qiwo_webdav_secret_schema,
      SECRET_COLLECTION_DEFAULT,
      "Qiwo WebDAV password",
      password,
      NULL,
      &error,
      QIWO_SECRET_ATTRIBUTE_SERVICE,
      QIWO_SECRET_ATTRIBUTE_VALUE,
      NULL);
#else
  return FALSE;
#endif
}

static gchar *
try_load_secret_password(void)
{
  if (!secret_service_allowed()) {
    return NULL;
  }
#ifdef QIWO_WITH_LIBSECRET
  g_autoptr(GError) error = NULL;
  return secret_password_lookup_sync(
      &qiwo_webdav_secret_schema,
      NULL,
      &error,
      QIWO_SECRET_ATTRIBUTE_SERVICE,
      QIWO_SECRET_ATTRIBUTE_VALUE,
      NULL);
#else
  return NULL;
#endif
}

static void
try_delete_secret_password(void)
{
#ifdef QIWO_WITH_LIBSECRET
  if (!secret_service_allowed()) return;
  g_autoptr(GError) error = NULL;
  secret_password_clear_sync(
      &qiwo_webdav_secret_schema,
      NULL,
      &error,
      QIWO_SECRET_ATTRIBUTE_SERVICE,
      QIWO_SECRET_ATTRIBUTE_VALUE,
      NULL);
#endif
}

gboolean
qiwo_webdav_config_load(QiwoWebDavSettings *settings, GError **error)
{
  g_return_val_if_fail(settings != NULL, FALSE);

  g_autofree gchar *path = qiwo_webdav_config_get_file_path();
  if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
    return TRUE;
  }

  g_autoptr(GKeyFile) key_file = g_key_file_new();
  if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, error)) {
    return FALSE;
  }

  g_clear_pointer(&settings->url, g_free);
  g_clear_pointer(&settings->username, g_free);
  g_clear_pointer(&settings->password, g_free);
  g_clear_pointer(&settings->device_id, g_free);

  settings->url = get_optional_string(key_file, "url");
  settings->username = get_optional_string(key_file, "username");
  settings->device_id = get_optional_string(key_file, "device_id");
  settings->password = get_optional_string(key_file, "password");
  settings->auto_sync_interval_minutes =
      g_key_file_get_uint64(key_file, QIWO_CONFIG_GROUP,
                            "auto_sync_interval_minutes", NULL);
  if (settings->password && settings->password[0]) {
    settings->password_storage_mode = QIWO_PASSWORD_STORAGE_LOCAL_FILE;
  } else {
    settings->password = try_load_secret_password();
    settings->password_storage_mode =
        settings->password ? QIWO_PASSWORD_STORAGE_SECRET_SERVICE :
        QIWO_PASSWORD_STORAGE_NONE;
  }
  return TRUE;
}

gboolean
qiwo_webdav_config_save(const QiwoWebDavSettings *settings, GError **error)
{
  g_return_val_if_fail(settings != NULL, FALSE);

  g_autofree gchar *path = qiwo_webdav_config_get_file_path();
  g_autofree gchar *dir = g_path_get_dirname(path);
  if (g_mkdir_with_parents(dir, 0700) != 0) {
    g_set_error(error, QIWO_WEBDAV_CONFIG_ERROR,
                QIWO_WEBDAV_CONFIG_ERROR_SAVE_FAILED,
                "Unable to create config directory: %s", dir);
    return FALSE;
  }

  g_autoptr(GKeyFile) key_file = g_key_file_new();
  set_string_if_present(key_file, "url", settings->url);
  set_string_if_present(key_file, "username", settings->username);
  set_string_if_present(key_file, "device_id", settings->device_id);
  gboolean stored_in_secret = try_store_secret_password(settings->password);
  if (settings->password && settings->password[0] && !stored_in_secret) {
    g_key_file_set_string(key_file, QIWO_CONFIG_GROUP, "password", settings->password);
    g_key_file_set_string(key_file, QIWO_CONFIG_GROUP,
                          "password_storage_mode", "local-file");
  } else if (stored_in_secret) {
    g_key_file_set_string(key_file, QIWO_CONFIG_GROUP,
                          "password_storage_mode", "secret-service");
  }
  g_key_file_set_uint64(key_file, QIWO_CONFIG_GROUP,
                        "auto_sync_interval_minutes",
                        settings->auto_sync_interval_minutes);

  gsize length = 0;
  g_autofree gchar *data = g_key_file_to_data(key_file, &length, error);
  if (!data) return FALSE;

  if (!g_file_set_contents(path, data, (gssize)length, error)) {
    return FALSE;
  }
  g_chmod(path, 0600);
  return TRUE;
}

gboolean
qiwo_webdav_config_delete_password(GError **error)
{
  try_delete_secret_password();

  g_autofree gchar *path = qiwo_webdav_config_get_file_path();
  if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
    return TRUE;
  }

  g_autoptr(GKeyFile) key_file = g_key_file_new();
  if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, error)) {
    return FALSE;
  }
  g_key_file_remove_key(key_file, QIWO_CONFIG_GROUP, "password", NULL);
  g_key_file_remove_key(key_file, QIWO_CONFIG_GROUP,
                        "password_storage_mode", NULL);

  gsize length = 0;
  g_autofree gchar *data = g_key_file_to_data(key_file, &length, error);
  if (!data) return FALSE;
  if (!g_file_set_contents(path, data, (gssize)length, error)) {
    return FALSE;
  }
  g_chmod(path, 0600);
  return TRUE;
}

static gboolean
env_to_uint(const gchar *value, guint *out)
{
  if (!value || !value[0]) return FALSE;
  gchar *endptr = NULL;
  guint64 parsed = g_ascii_strtoull(value, &endptr, 10);
  if (!endptr || *endptr != '\0' || parsed > G_MAXUINT) return FALSE;
  *out = (guint)parsed;
  return TRUE;
}

static void
copy_saved_to_effective(const QiwoWebDavSettings *saved,
                        QiwoEffectiveWebDavSettings *effective)
{
  effective->url = g_strdup(saved->url);
  effective->username = g_strdup(saved->username);
  effective->password = g_strdup(saved->password);
  effective->device_id = g_strdup(saved->device_id);
  effective->auto_sync_interval_minutes = saved->auto_sync_interval_minutes;
  effective->password_storage_mode = saved->password_storage_mode;
}

gboolean
qiwo_webdav_config_load_effective(QiwoEffectiveWebDavSettings *settings,
                                  GError **error)
{
  g_return_val_if_fail(settings != NULL, FALSE);

  QiwoWebDavSettings saved;
  qiwo_webdav_settings_init(&saved);
  if (!qiwo_webdav_config_load(&saved, error)) {
    qiwo_webdav_settings_clear(&saved);
    return FALSE;
  }

  copy_saved_to_effective(&saved, settings);

  const gchar *value = g_getenv(QIWO_WEBDAV_ENV_URL);
  if (value && value[0]) {
    g_free(settings->url);
    settings->url = g_strdup(value);
    settings->url_overridden = TRUE;
  }

  value = g_getenv(QIWO_WEBDAV_ENV_USERNAME);
  if (value && value[0]) {
    g_free(settings->username);
    settings->username = g_strdup(value);
    settings->username_overridden = TRUE;
  }

  value = g_getenv(QIWO_WEBDAV_ENV_PASSWORD);
  if (value && value[0]) {
    g_free(settings->password);
    settings->password = g_strdup(value);
    settings->password_overridden = TRUE;
    settings->password_storage_mode = QIWO_PASSWORD_STORAGE_NONE;
  }

  value = g_getenv(QIWO_WEBDAV_ENV_DEVICE_ID);
  if (value && value[0]) {
    g_free(settings->device_id);
    settings->device_id = g_strdup(value);
    settings->device_id_overridden = TRUE;
  }

  value = g_getenv(QIWO_WEBDAV_ENV_AUTO_SYNC_INTERVAL);
  guint interval = 0;
  if (env_to_uint(value, &interval)) {
    settings->auto_sync_interval_minutes = interval;
    settings->auto_sync_interval_overridden = TRUE;
  }

  qiwo_webdav_settings_clear(&saved);
  return TRUE;
}

gboolean
qiwo_webdav_effective_settings_validate(const QiwoEffectiveWebDavSettings *settings,
                                        GError **error)
{
  g_return_val_if_fail(settings != NULL, FALSE);
  if (!settings->url || !settings->url[0]) {
    g_set_error(error, QIWO_WEBDAV_CONFIG_ERROR,
                QIWO_WEBDAV_CONFIG_ERROR_MISSING_URL,
                "WebDAV URL is required.");
    return FALSE;
  }
  if (!settings->username || !settings->username[0]) {
    g_set_error(error, QIWO_WEBDAV_CONFIG_ERROR,
                QIWO_WEBDAV_CONFIG_ERROR_MISSING_USERNAME,
                "WebDAV username is required.");
    return FALSE;
  }
  if (!settings->password || !settings->password[0]) {
    g_set_error(error, QIWO_WEBDAV_CONFIG_ERROR,
                QIWO_WEBDAV_CONFIG_ERROR_MISSING_PASSWORD,
                "WebDAV password is required.");
    return FALSE;
  }
  if (!settings->device_id || !settings->device_id[0]) {
    g_set_error(error, QIWO_WEBDAV_CONFIG_ERROR,
                QIWO_WEBDAV_CONFIG_ERROR_MISSING_DEVICE_ID,
                "Device ID is required.");
    return FALSE;
  }
  return TRUE;
}

void
qiwo_webdav_config_set_secret_service_available_for_tests(gboolean available)
{
  secret_service_available_override_set = TRUE;
  secret_service_available_override = available;
}

void
qiwo_webdav_config_reset_secret_service_available_for_tests(void)
{
  secret_service_available_override_set = FALSE;
  secret_service_available_override = TRUE;
}
