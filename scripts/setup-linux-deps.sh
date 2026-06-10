#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/setup-linux-deps.sh [options]

Verify and optionally install Linux build dependencies for qiwo-ibusr.

Options:
  --need-cargo    Require cargo and rustc for building qiwo-sync-core
  --need-git      Require git for initializing missing submodules
  --no-install    Only verify dependencies; do not invoke a package manager
  -h, --help      Show this help
EOF
}

need_cargo=0
need_git=0
auto_install=1
min_rust_version="1.85.0"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --need-cargo)
      need_cargo=1
      shift
      ;;
    --need-git)
      need_git=1
      shift
      ;;
    --no-install)
      auto_install=0
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
  echo "ERROR: setup-linux-deps.sh must be run on Linux." >&2
  exit 1
fi

missing=()

has_command() {
  command -v "$1" >/dev/null 2>&1
}

has_c_compiler() {
  has_command cc || has_command gcc || has_command clang
}

extract_semver() {
  "$1" --version 2>/dev/null | sed -E 's/^[^0-9]*([0-9]+)\.([0-9]+)\.([0-9]+).*/\1.\2.\3/'
}

version_ge() {
  local current="$1"
  local required="$2"
  local current_major current_minor current_patch
  local required_major required_minor required_patch

  IFS=. read -r current_major current_minor current_patch <<<"$current"
  IFS=. read -r required_major required_minor required_patch <<<"$required"

  current_major="${current_major:-0}"
  current_minor="${current_minor:-0}"
  current_patch="${current_patch:-0}"
  required_major="${required_major:-0}"
  required_minor="${required_minor:-0}"
  required_patch="${required_patch:-0}"

  if (( current_major > required_major )); then
    return 0
  elif (( current_major < required_major )); then
    return 1
  elif (( current_minor > required_minor )); then
    return 0
  elif (( current_minor < required_minor )); then
    return 1
  fi
  (( current_patch >= required_patch ))
}

has_min_version() {
  local command_name="$1"
  local required="$2"
  local current

  current="$(extract_semver "$command_name")"
  [[ "$current" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || return 1
  version_ge "$current" "$required"
}

add_missing() {
  missing+=("$1")
}

collect_missing_dependencies() {
  missing=()

  has_command cmake || add_missing "cmake"
  has_command make || add_missing "make"
  has_c_compiler || add_missing "C compiler (cc/gcc/clang)"

  if ! has_command pkg-config; then
    add_missing "pkg-config"
  else
    local pkg
    for pkg in ibus-1.0 rime libnotify gtk+-3.0 libsecret-1; do
      pkg-config --exists "$pkg" 2>/dev/null || add_missing "pkg-config module $pkg"
    done
  fi

  if [[ $need_cargo -eq 1 ]]; then
    if ! has_command cargo; then
      add_missing "cargo >= $min_rust_version"
    elif ! has_min_version cargo "$min_rust_version"; then
      add_missing "cargo >= $min_rust_version (found $(extract_semver cargo))"
    fi

    if ! has_command rustc; then
      add_missing "rustc >= $min_rust_version"
    elif ! has_min_version rustc "$min_rust_version"; then
      add_missing "rustc >= $min_rust_version (found $(extract_semver rustc))"
    fi
  fi

  if [[ $need_git -eq 1 ]]; then
    has_command git || add_missing "git"
  fi
}

print_missing_dependencies() {
  local dep
  for dep in "${missing[@]}"; do
    echo "  - $dep" >&2
  done
}

print_rust_toolchain_hint() {
  if [[ $need_cargo -ne 1 ]]; then
    return 0
  fi

  echo "qiwo-sync-core uses Rust 2024 edition and requires Rust/Cargo >= $min_rust_version." >&2
  echo "Older distro packages, such as Debian 10 cargo/rustc, may be too old even after apt install." >&2
  echo "Install a current Rust toolchain with rustup and ensure ~/.cargo/bin is in PATH, or pass --sync-bin PATH." >&2
}

run_as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  else
    local sudo_cmd="${SUDO:-sudo}"
    if ! command -v "${sudo_cmd%% *}" >/dev/null 2>&1; then
      echo "ERROR: sudo is required to install missing dependencies." >&2
      exit 1
    fi
    $sudo_cmd "$@"
  fi
}

install_known_packages() {
  local packages=()

  if has_command apt; then
    packages=(
      ibus libibus-1.0-dev
      librime-dev librime-data
      libnotify-dev libgtk-3-dev libsecret-1-dev
      cmake make gcc pkg-config
    )
    [[ $need_cargo -eq 1 ]] && packages+=(cargo rustc)
    [[ $need_git -eq 1 ]] && packages+=(git)
    run_as_root apt install -y "${packages[@]}"
  elif has_command apt-get; then
    packages=(
      ibus libibus-1.0-dev
      librime-dev librime-data
      libnotify-dev libgtk-3-dev libsecret-1-dev
      cmake make gcc pkg-config
    )
    [[ $need_cargo -eq 1 ]] && packages+=(cargo rustc)
    [[ $need_git -eq 1 ]] && packages+=(git)
    run_as_root apt-get install -y "${packages[@]}"
  elif has_command dnf; then
    packages=(
      ibus ibus-devel
      librime librime-devel rime-data
      libnotify-devel gtk3-devel libsecret-devel
      cmake make gcc pkg-config
    )
    [[ $need_cargo -eq 1 ]] && packages+=(cargo rust)
    [[ $need_git -eq 1 ]] && packages+=(git)
    run_as_root dnf install -y "${packages[@]}"
  elif has_command pacman; then
    packages=(
      ibus
      librime rime-data
      libnotify gtk3 libsecret
      cmake make gcc pkgconf
    )
    [[ $need_cargo -eq 1 ]] && packages+=(rust)
    [[ $need_git -eq 1 ]] && packages+=(git)
    run_as_root pacman -S --needed --noconfirm "${packages[@]}"
  elif has_command zypper; then
    packages=(
      ibus ibus-devel
      librime-devel rime-data
      libnotify-devel gtk3-devel libsecret-devel
      cmake make gcc pkg-config
    )
    [[ $need_cargo -eq 1 ]] && packages+=(cargo rust)
    [[ $need_git -eq 1 ]] && packages+=(git)
    run_as_root zypper install -y "${packages[@]}"
  else
    echo "ERROR: unknown package manager." >&2
    echo "Install IBus, librime development headers, Rime data, GTK 3, libnotify, libsecret, CMake, make, a C compiler, pkg-config, and the missing tools above manually." >&2
    exit 1
  fi
}

echo "*** Checking system dependencies..."
collect_missing_dependencies
if [[ ${#missing[@]} -eq 0 ]]; then
  echo "--> All system dependencies present."
  exit 0
fi

echo "--> Missing system dependencies:" >&2
print_missing_dependencies
print_rust_toolchain_hint

if [[ $auto_install -eq 0 ]]; then
  echo "ERROR: dependency auto-install is disabled." >&2
  echo "Install the missing dependencies manually or rerun without --skip-setup." >&2
  exit 1
fi

echo "--> Installing missing system dependencies..."
install_known_packages

collect_missing_dependencies
if [[ ${#missing[@]} -ne 0 ]]; then
  echo "ERROR: dependencies are still missing after package installation:" >&2
  print_missing_dependencies
  print_rust_toolchain_hint
  exit 1
fi

echo "--> All system dependencies present."
