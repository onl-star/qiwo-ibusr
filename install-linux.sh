#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: ./install-linux.sh [options]

Build and install qiwo-ibusr on Linux.

Options:
  --prefix PATH          Install prefix, default: /usr
  --build-dir PATH       Build directory, default: build
  --rime-data-dir PATH   Override Rime data directory if CMake cannot find it
  --run-tests            Run CTest before installation
  --skip-setup           Do not auto-install system dependencies
  --skip-ibus-restart    Do not restart IBus after installation
  -h, --help             Show this help

Environment:
  PREFIX                 Same as --prefix
  builddir               Same as --build-dir
  RIME_DATA_DIR          Same as --rime-data-dir
EOF
}

prefix="${PREFIX:-/usr}"
build_dir="${builddir:-build}"
rime_data_dir="${RIME_DATA_DIR:-}"
run_tests=0
skip_setup=0
skip_ibus_restart=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      prefix="${2:?missing value for --prefix}"
      shift 2
      ;;
    --build-dir)
      build_dir="${2:?missing value for --build-dir}"
      shift 2
      ;;
    --rime-data-dir)
      rime_data_dir="${2:?missing value for --rime-data-dir}"
      shift 2
      ;;
    --run-tests)
      run_tests=1
      shift
      ;;
    --skip-setup)
      skip_setup=1
      shift
      ;;
    --skip-ibus-restart)
      skip_ibus_restart=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "ERROR: install-linux.sh must be run on Linux." >&2
  exit 1
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_path="$build_dir"
if [[ "$build_path" != /* ]]; then
  build_path="$script_dir/$build_path"
fi

if [[ $skip_setup -eq 0 ]]; then
  make -C "$script_dir" setup
fi

cmake_args=(
  -S "$script_dir"
  -B "$build_path"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="$prefix"
  -DCMAKE_INSTALL_DATADIR="$prefix/share"
  -DCMAKE_INSTALL_LIBEXECDIR="$prefix/lib"
  -DBUILD_TESTING=ON
)

if [[ -n "$rime_data_dir" ]]; then
  cmake_args+=("-DRIME_DATA_DIR=$rime_data_dir")
fi

cmake "${cmake_args[@]}"
cmake --build "$build_path"

if [[ $run_tests -eq 1 ]]; then
  (cd "$build_path" && ctest --output-on-failure)
fi

can_install_without_sudo=0
if [[ "$(id -u)" -eq 0 ]]; then
  can_install_without_sudo=1
elif [[ -d "$prefix" && -w "$prefix" ]]; then
  can_install_without_sudo=1
elif [[ ! -e "$prefix" ]]; then
  prefix_parent="$(dirname -- "$prefix")"
  if [[ -d "$prefix_parent" && -w "$prefix_parent" ]]; then
    can_install_without_sudo=1
  fi
fi

if [[ $can_install_without_sudo -eq 1 ]]; then
  cmake --build "$build_path" --target install
else
  if ! command -v sudo >/dev/null 2>&1; then
    echo "ERROR: sudo is required for installing to $prefix. Re-run as root or use --prefix." >&2
    exit 1
  fi
  sudo cmake --build "$build_path" --target install
fi

component_file="$prefix/share/ibus/component/qiwo.xml"
engine_file="$prefix/lib/qiwo/ibus-engine-rime"
settings_file="$prefix/bin/qiwo-webdav-settings"
settings_desktop_file="$prefix/share/applications/qiwo-webdav-settings.desktop"
sync_file="$prefix/share/qiwo/qiwo-rime-sync"
if [[ ! -f "$component_file" ]]; then
  echo "ERROR: IBus component was not installed: $component_file" >&2
  echo "Installed qiwo.xml files under $prefix:" >&2
  find "$prefix" -path '*/ibus/component/qiwo.xml' -print 2>/dev/null || true
  exit 1
fi
if [[ ! -x "$engine_file" ]]; then
  echo "ERROR: IBus engine executable was not installed or is not executable: $engine_file" >&2
  exit 1
fi
if [[ ! -x "$settings_file" ]]; then
  echo "ERROR: WebDAV settings executable was not installed or is not executable: $settings_file" >&2
  exit 1
fi
if [[ ! -f "$settings_desktop_file" ]]; then
  echo "ERROR: WebDAV settings desktop entry was not installed: $settings_desktop_file" >&2
  exit 1
fi
if [[ ! -x "$sync_file" ]]; then
  echo "ERROR: WebDAV sync executable was not installed or is not executable: $sync_file" >&2
  exit 1
fi

if [[ $skip_ibus_restart -eq 0 ]]; then
  if command -v ibus >/dev/null 2>&1; then
    ibus restart || echo "WARNING: failed to restart IBus; restart it manually." >&2
  else
    echo "WARNING: ibus command not found; restart IBus manually after installing." >&2
  fi
fi

cat <<EOF

Installed qiwo-ibusr to $prefix.

Next steps:
  1. Run ibus-setup and add Qiwo/Rime if it is not already listed.
  2. Switch to the input method from your desktop input source menu.
  3. Open "WebDAV 设置" from the IBus panel to configure sync.

Installed files checked:
  $component_file
  $engine_file
  $settings_file
  $settings_desktop_file
  $sync_file
EOF
