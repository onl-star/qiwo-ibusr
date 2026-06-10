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
if [[ "$run_sync_calls" -lt 2 ]]; then
  echo "settings Test Connection and Sync Now no longer use qiwo_sync_command_run_sync" >&2
  exit 1
fi

grep -q 'Sync Config Now' "$settings_file" || {
  echo "settings window should label standalone sync as configuration-only" >&2
  exit 1
}

grep -q 'User dictionaries require the IBus panel WebDAV Sync action' "$settings_file" || {
  echo "settings window should explain that user dictionary sync runs through the IBus panel" >&2
  exit 1
}

grep -q 'ibus_rime_sync_user_data();' "$rime_main_file" || {
  echo "auto-sync or panel path no longer calls ibus_rime_sync_user_data" >&2
  exit 1
}
