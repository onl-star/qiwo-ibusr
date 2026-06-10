#include "rime_config.h"
#include "qiwo_webdav_config.h"
#include "rime_settings.h"
#include <rime_api.h>
#include <string.h>

extern RimeApi *rime_api;

static struct ColorSchemeDefinition preset_color_schemes[] = {
  { "aqua", 0xffffff, 0x0a3dfa },
  { "azure", 0xffffff, 0x0a3dea },
  { "ink", 0xffffff, 0x000000 },
  { "luna", 0x000000, 0xffff7f },
  { NULL, 0, 0 }
};

static struct IBusRimeSettings ibus_rime_settings_default = {
  .embed_preedit_text = TRUE,
  .preedit_style = PREEDIT_STYLE_COMPOSITION,
  .cursor_type = CURSOR_TYPE_INSERT,
  .lookup_table_orientation = IBUS_ORIENTATION_SYSTEM,
  .color_scheme = NULL,
  .auto_sync_interval_seconds = 0,
  .auto_commit_spacing_enabled = TRUE,
};

struct IBusRimeSettings g_ibus_rime_settings;

static guint
interval_minutes_to_seconds(guint interval_minutes)
{
  if (interval_minutes == 0) {
    return 0;
  }
  if (interval_minutes > G_MAXUINT / 60) {
    return G_MAXUINT;
  }
  return interval_minutes * 60;
}

static void
select_color_scheme(struct IBusRimeSettings* settings,
		    const char* color_scheme_id)
{
  struct ColorSchemeDefinition* c;
  for (c = preset_color_schemes; c->color_scheme_id; ++c) {
    if (!strcmp(c->color_scheme_id, color_scheme_id)) {
      settings->color_scheme = c;
      g_debug("selected color scheme: %s", color_scheme_id);
      return;
    }
  }
  // fallback to default
  settings->color_scheme = NULL;
}

void
ibus_rime_load_settings()
{
  g_ibus_rime_settings = ibus_rime_settings_default;

  RimeConfig config = {0};
  if (!rime_api->config_open("ibus_rime", &config)) {
    g_error("error loading settings for ibus_rime");
    return;
  }

  Bool inline_preedit = False;
  if (rime_api->config_get_bool(
          &config, "style/inline_preedit", &inline_preedit)) {
    g_ibus_rime_settings.embed_preedit_text = !!inline_preedit;
  }

  const char* preedit_style_str =
      rime_api->config_get_cstring(&config, "style/preedit_style");
  if(preedit_style_str) {
    if(!strcmp(preedit_style_str, "composition")) {
      g_ibus_rime_settings.preedit_style = PREEDIT_STYLE_COMPOSITION;
    } else if(!strcmp(preedit_style_str, "preview")) {
      g_ibus_rime_settings.preedit_style = PREEDIT_STYLE_PREVIEW;
    }
  }

  const char* cursor_type_str =
      rime_api->config_get_cstring(&config, "style/cursor_type");
  if (cursor_type_str) {
    if (!strcmp(cursor_type_str, "insert")) {
      g_ibus_rime_settings.cursor_type = CURSOR_TYPE_INSERT;
    } else if (!strcmp(cursor_type_str, "select")) {
      g_ibus_rime_settings.cursor_type = CURSOR_TYPE_SELECT;
    }
  }

  Bool horizontal = False;
  if (rime_api->config_get_bool(&config, "style/horizontal", &horizontal)) {
    g_ibus_rime_settings.lookup_table_orientation =
      horizontal ? IBUS_ORIENTATION_HORIZONTAL : IBUS_ORIENTATION_VERTICAL;
  }

  Bool auto_commit_spacing = True;
  if (rime_api->config_get_bool(
          &config, "input/auto_commit_spacing", &auto_commit_spacing)) {
    g_ibus_rime_settings.auto_commit_spacing_enabled = !!auto_commit_spacing;
  }

  const char* color_scheme =
    rime_api->config_get_cstring(&config, "style/color_scheme");
  if (color_scheme) {
    select_color_scheme(&g_ibus_rime_settings, color_scheme);
  }

  // 读取自动同步词库间隔（分钟，0 = 禁用）
  int interval_minutes = 0;
  gboolean rime_interval_configured = FALSE;
  if (rime_api->config_get_int(&config, "sync/auto_sync_interval_minutes", &interval_minutes)) {
    rime_interval_configured = TRUE;
    g_ibus_rime_settings.auto_sync_interval_seconds =
      interval_minutes > 0 ? interval_minutes_to_seconds((guint)interval_minutes) : 0;
  }

  rime_api->config_close(&config);

  gboolean webdav_interval_overridden = FALSE;
  g_autoptr(GError) error = NULL;
  guint webdav_interval_minutes =
      qiwo_webdav_config_get_effective_auto_sync_interval_minutes(
          &webdav_interval_overridden, &error);
  if (error) {
    g_warning("error loading WebDAV auto-sync interval: %s", error->message);
    return;
  }
  if (webdav_interval_overridden ||
      (!rime_interval_configured && webdav_interval_minutes > 0)) {
    g_ibus_rime_settings.auto_sync_interval_seconds =
      interval_minutes_to_seconds(webdav_interval_minutes);
  }
}
