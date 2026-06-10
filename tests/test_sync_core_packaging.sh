#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake_file="$root/CMakeLists.txt"
rime_main_file="$root/rime_main.c"
settings_file="$root/qiwo_webdav_settings.c"

if grep -q 'install(PROGRAMS qiwo_sync.py' "$cmake_file"; then
  echo "CMake still installs legacy qiwo_sync.py as qiwo-rime-sync" >&2
  exit 1
fi

grep -q 'QIWO_RIME_SYNC_BIN' "$cmake_file" || {
  echo "CMake does not expose QIWO_RIME_SYNC_BIN" >&2
  exit 1
}

grep -q 'QIWO_SYNC_CORE_DIR' "$cmake_file" || {
  echo "CMake does not expose QIWO_SYNC_CORE_DIR" >&2
  exit 1
}

grep -q 'qiwo_rime_default_config.c' "$cmake_file" || {
  echo "qiwo-webdav-settings does not include Rime default config support" >&2
  exit 1
}

grep -q 'target_link_libraries(qiwo-webdav-settings .*Rime' "$cmake_file" || {
  echo "qiwo-webdav-settings is not linked with librime for full Sync Now" >&2
  exit 1
}

grep -q 'qiwo-sync-core/Cargo.toml' "$cmake_file" || {
  echo "CMake does not prefer the bundled qiwo-sync-core submodule" >&2
  exit 1
}

sync_user_calls="$(grep -c 'rime_api->sync_user_data()' "$rime_main_file" || true)"
if [[ "$sync_user_calls" -lt 2 ]]; then
  echo "panel sync no longer exports and imports Rime user data around shared sync" >&2
  exit 1
fi

run_sync_calls="$(grep -c 'qiwo_sync_command_run_sync' "$settings_file" || true)"
if [[ "$run_sync_calls" -lt 1 ]]; then
  echo "settings Test Connection no longer uses qiwo_sync_command_run_sync" >&2
  exit 1
fi

grep -q 'Sync Now' "$settings_file" || {
  echo "settings window should expose full Sync Now action" >&2
  exit 1
}

grep -q 'qiwo_sync_command_run_full_sync' "$settings_file" || {
  echo "settings Sync Now should use the full sync flow with Rime user dictionary hooks" >&2
  exit 1
}

grep -q 'ibus_rime_sync_user_data();' "$rime_main_file" || {
  echo "auto-sync or panel path no longer calls ibus_rime_sync_user_data" >&2
  exit 1
}
