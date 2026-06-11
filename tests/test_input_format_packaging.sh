#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake_file="$root/CMakeLists.txt"
settings_header="$root/rime_settings.h"
settings_source="$root/rime_settings.c"
bridge_header="$root/qiwo_input_format_bridge.h"
bridge_source="$root/qiwo_input_format_bridge.c"
engine_source="$root/rime_engine.c"
config_file="$root/ibus_rime.yaml"
install_doc="$root/INSTALL.md"
gitmodules="$root/.gitmodules"

grep -q 'QIWO_INPUT_FORMAT_CORE_DIR' "$cmake_file" || {
  echo "CMake does not expose QIWO_INPUT_FORMAT_CORE_DIR" >&2
  exit 1
}

grep -q 'qiwo-input-format-core/Cargo.toml' "$cmake_file" || {
  echo "CMake does not prefer the bundled qiwo-input-format-core submodule" >&2
  exit 1
}

grep -q 'path = qiwo-input-format-core' "$gitmodules" || {
  echo ".gitmodules does not declare qiwo-input-format-core" >&2
  exit 1
}

grep -q 'url = https://github.com/LeaWron/qiwo-input-format-core.git' "$gitmodules" || {
  echo ".gitmodules does not use the canonical qiwo-input-format-core repository URL" >&2
  exit 1
}

grep -q 'qiwo_input_format_bridge.c' "$cmake_file" || {
  echo "CMake does not build the Linux input format bridge source" >&2
  exit 1
}

grep -q 'qiwo_input_format' "$cmake_file" || {
  echo "CMake does not build or link qiwo_input_format" >&2
  exit 1
}

grep -q 'QIWO_INPUT_FORMAT_OUTPUT' "$cmake_file" || {
  echo "CMake does not stage the qiwo_input_format library artifact" >&2
  exit 1
}

grep -Fq 'install(FILES "${QIWO_INPUT_FORMAT_OUTPUT}"' "$cmake_file" || {
  echo "CMake does not install the qiwo_input_format shared library" >&2
  exit 1
}

grep -q 'input_format_packaging' "$cmake_file" || {
  echo "CMake does not register the input format packaging smoke test" >&2
  exit 1
}

test -f "$bridge_header" || {
  echo "Linux input format bridge header is missing" >&2
  exit 1
}

test -f "$bridge_source" || {
  echo "Linux input format bridge source is missing" >&2
  exit 1
}

grep -q 'qiwo_input_format_commit_text' "$bridge_source" || {
  echo "Linux bridge does not call the shared C ABI formatter" >&2
  exit 1
}

grep -q 'qiwo_input_format_free_string' "$bridge_source" || {
  echo "Linux bridge does not free formatted strings with the shared C ABI" >&2
  exit 1
}

grep -q 'auto_commit_spacing_enabled' "$settings_header" || {
  echo "rime settings do not expose auto_commit_spacing_enabled" >&2
  exit 1
}

grep -q 'auto_commit_spacing_enabled = TRUE' "$settings_source" || {
  echo "auto commit spacing does not default to enabled" >&2
  exit 1
}

grep -q 'input/auto_commit_spacing' "$settings_source" || {
  echo "rime settings do not load input/auto_commit_spacing" >&2
  exit 1
}

grep -q 'qiwo_input_format_bridge_format_commit_text' "$engine_source" || {
  echo "rime engine does not format commit text through the input format bridge" >&2
  exit 1
}

grep -Fq 'ibus_engine_commit_text((IBusEngine *)rime_engine, text)' "$engine_source" || {
  echo "rime engine commit call was unexpectedly removed" >&2
  exit 1
}

grep -q 'auto_commit_spacing: true' "$config_file" || {
  echo "ibus_rime.yaml does not document default-enabled input/auto_commit_spacing" >&2
  exit 1
}

grep -q 'input/auto_commit_spacing' "$install_doc" || {
  echo "INSTALL.md does not document input/auto_commit_spacing" >&2
  exit 1
}
