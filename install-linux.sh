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
  --sync-bin PATH        Use a prebuilt shared qiwo-rime-sync executable
  --sync-core-dir PATH   Build shared qiwo-rime-sync from this qiwo-sync-core tree
  --run-tests            Run CTest before installation
  --skip-setup           Do not auto-install system dependencies
  --skip-ibus-restart    Do not restart IBus after installation
  -h, --help             Show this help

Environment:
  PREFIX                 Same as --prefix
  builddir               Same as --build-dir
  RIME_DATA_DIR          Same as --rime-data-dir
  QIWO_RIME_SYNC_BIN     Same as --sync-bin
  QIWO_SYNC_CORE_DIR     Same as --sync-core-dir
EOF
}

prefix="${PREFIX:-/usr}"
build_dir="${builddir:-build}"
rime_data_dir="${RIME_DATA_DIR:-}"
sync_bin="${QIWO_RIME_SYNC_BIN:-}"
sync_core_dir="${QIWO_SYNC_CORE_DIR:-}"
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
    --sync-bin)
      sync_bin="${2:?missing value for --sync-bin}"
      shift 2
      ;;
    --sync-core-dir)
      sync_core_dir="${2:?missing value for --sync-core-dir}"
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

ensure_bundled_sync_core() {
  if [[ -n "$sync_bin" || -n "$sync_core_dir" ]]; then
    return 0
  fi
  if [[ -f "$script_dir/qiwo-sync-core/Cargo.toml" ]]; then
    return 0
  fi
  if [[ -f "$script_dir/../qiwo-sync-core/Cargo.toml" ]]; then
    return 0
  fi
  if [[ ! -f "$script_dir/.gitmodules" ]]; then
    return 0
  fi
  if ! command -v git >/dev/null 2>&1; then
    echo "ERROR: bundled qiwo-sync-core is missing and git is not available." >&2
    echo "Run git submodule update --init --recursive, install git, or pass --sync-bin PATH." >&2
    exit 1
  fi

  echo "--> qiwo-sync-core submodule missing, initializing..."
  if ! git -C "$script_dir" submodule update --init --recursive qiwo-sync-core; then
    echo "ERROR: failed to initialize qiwo-sync-core submodule." >&2
    echo "Run git submodule update --init --recursive, or pass --sync-bin PATH." >&2
    exit 1
  fi
  if [[ ! -f "$script_dir/qiwo-sync-core/Cargo.toml" ]]; then
    echo "ERROR: qiwo-sync-core submodule is still missing after initialization." >&2
    echo "Expected: $script_dir/qiwo-sync-core/Cargo.toml" >&2
    exit 1
  fi
}

ensure_bundled_sync_core

detect_rime_data_dir() {
  local candidates=(
    "$prefix/share/rime-data"
    "$prefix/share/rime/data"
    "$prefix/share/brise"
    /usr/local/share/rime-data
    /usr/local/share/rime/data
    /usr/local/share/brise
    /usr/share/rime-data
    /usr/share/rime/data
    /usr/share/brise
  )
  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -d "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  if command -v pkg-config >/dev/null 2>&1; then
    local var value
    for var in pkgdatadir datadir rime_data_dir; do
      value="$(pkg-config --variable="$var" rime 2>/dev/null || true)"
      if [[ -n "$value" && -d "$value" ]]; then
        printf '%s\n' "$value"
        return 0
      fi
    done
  fi

  if command -v dpkg-query >/dev/null 2>&1; then
    value="$(dpkg-query -L librime-data 2>/dev/null | while IFS= read -r path; do
      if [[ -d "$path" && -f "$path/default.yaml" ]]; then
        printf '%s\n' "$path"
        break
      fi
    done)"
    if [[ -n "$value" ]]; then
      printf '%s\n' "$value"
      return 0
    fi
  fi

  return 1
}

if [[ -z "$rime_data_dir" ]]; then
  rime_data_dir="$(detect_rime_data_dir || true)"
fi
if [[ -z "$rime_data_dir" || ! -d "$rime_data_dir" ]]; then
  echo "ERROR: Rime data directory was not found." >&2
  echo "Install rime data packages or rerun with --rime-data-dir PATH." >&2
  echo "Useful diagnostics:" >&2
  echo "  dpkg-query -L librime-data | grep -E 'default.yaml|rime-data|brise'" >&2
  echo "  find /usr /usr/local -maxdepth 4 -name default.yaml 2>/dev/null" >&2
  exit 1
fi

cmake_args=(
  -S "$script_dir"
  -B "$build_path"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="$prefix"
  -DCMAKE_INSTALL_DATADIR="$prefix/share"
  -DCMAKE_INSTALL_LIBEXECDIR="$prefix/lib"
  -DBUILD_TESTING=ON
  "-DRIME_DATA_DIR=$rime_data_dir"
)
if [[ -n "$sync_bin" ]]; then
  if [[ ! -x "$sync_bin" ]]; then
    echo "ERROR: --sync-bin must point to an executable shared qiwo-rime-sync: $sync_bin" >&2
    exit 1
  fi
  sync_bin="$(cd -- "$(dirname -- "$sync_bin")" && pwd)/$(basename -- "$sync_bin")"
  cmake_args+=("-DQIWO_RIME_SYNC_BIN=$sync_bin")
fi
if [[ -n "$sync_core_dir" ]]; then
  if [[ ! -f "$sync_core_dir/Cargo.toml" ]]; then
    echo "ERROR: --sync-core-dir must point to qiwo-sync-core containing Cargo.toml: $sync_core_dir" >&2
    exit 1
  fi
  sync_core_dir="$(cd -- "$sync_core_dir" && pwd)"
  if [[ -z "$sync_bin" ]] && ! command -v cargo >/dev/null 2>&1; then
    echo "ERROR: cargo is required to build qiwo-sync-core. Install Rust/Cargo or pass --sync-bin PATH." >&2
    exit 1
  fi
  cmake_args+=("-DQIWO_SYNC_CORE_DIR=$sync_core_dir")
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
rime_frost_schema_file="$rime_data_dir/rime_frost.schema.yaml"
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
if ! sync_help="$("$sync_file" --help 2>&1)"; then
  echo "ERROR: installed qiwo-rime-sync did not run --help successfully: $sync_file" >&2
  echo "$sync_help" >&2
  exit 1
fi
if ! grep -Eq 'init-frost|sync-user-dict' <<<"$sync_help"; then
  echo "ERROR: installed qiwo-rime-sync does not look like the shared qiwo-sync-core CLI: $sync_file" >&2
  echo "Expected --help output to mention init-frost or sync-user-dict." >&2
  exit 1
fi
if [[ ! -f "$rime_frost_schema_file" ]]; then
  echo "ERROR: rime-frost schema was not installed: $rime_frost_schema_file" >&2
  echo "Make sure the rime-frost submodule is initialized:" >&2
  echo "  git submodule update --init --recursive" >&2
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
  $rime_frost_schema_file
EOF
