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

grep -q 'qiwo_sync_ipc.c' "$cmake_file" || {
  echo "CMake does not build the WebDAV sync IPC helper" >&2
  exit 1
}

grep -q 'qiwo-sync-core/Cargo.toml' "$cmake_file" || {
  echo "CMake does not prefer the bundled qiwo-sync-core submodule" >&2
  exit 1
}

grep -q 'qiwo_sync_command_run_full_sync' "$rime_main_file" || {
  echo "engine sync no longer runs the shared full sync flow" >&2
  exit 1
}

grep -q 'rime_api->sync_user_data()' "$rime_main_file" || {
  echo "engine full sync no longer calls Rime sync_user_data" >&2
  exit 1
}

grep -q 'join_maintenance_thread' "$rime_main_file" || {
  echo "engine full sync should wait for Rime user dictionary export/import" >&2
  exit 1
}

grep -q 'ibus_rime_redeploy_after_sync' "$rime_main_file" || {
  echo "engine full sync no longer redeploys Rime after WebDAV sync" >&2
  exit 1
}

grep -q 'start_maintenance((Bool)TRUE)' "$rime_main_file" || {
  echo "engine full sync should trigger a full Rime redeploy after sync" >&2
  exit 1
}

grep -q 'qiwo_sync_ipc_create_server' "$rime_main_file" || {
  echo "engine does not expose the settings Sync Now IPC server" >&2
  exit 1
}

run_sync_calls="$(grep -c 'qiwo_sync_command_run_sync' "$settings_file" || true)"
if [[ "$run_sync_calls" -lt 1 ]]; then
  echo "settings Test Connection no longer uses qiwo_sync_command_run_sync" >&2
  exit 1
fi

grep -q 'Sync Now' "$settings_file" || {
  echo "settings window should expose full Sync Now action" >&2
  exit 1
}

grep -q 'qiwo_sync_ipc_request_sync' "$settings_file" || {
  echo "settings Sync Now should delegate full sync to the running IBus engine" >&2
  exit 1
}

if grep -q 'rime_api.h' "$settings_file"; then
  echo "settings Sync Now should not initialize librime directly" >&2
  exit 1
fi

if grep -q 'sync_user_data' "$settings_file"; then
  echo "settings Sync Now should not touch Rime userdb directly" >&2
  exit 1
fi

grep -q 'ibus_rime_sync_user_data();' "$rime_main_file" || {
  echo "auto-sync or panel path no longer calls ibus_rime_sync_user_data" >&2
  exit 1
}
