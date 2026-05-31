#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[OK]${NC} $1"; }
warn()  { echo -e "${YELLOW}[--]${NC} $1"; }
err()   { echo -e "${RED}[ERR]${NC} $1"; }
header(){ echo -e "\n${YELLOW}=== $1 ===${NC}"; }

# ── 1. detect package manager ──────────────────────────────────

header "Detecting package manager"

detect_pm() {
  for pm in apt dnf pacman zypper; do
    command -v $pm &>/dev/null && { echo "$pm"; return; }
  done
  echo "unknown"
}

PM=$(detect_pm)
info "Package manager: $PM"

# ── 2. install system dependencies ─────────────────────────────

header "Installing system dependencies"

install_pkgs() {
  case $PM in
    apt)
      sudo apt update -qq
      sudo apt install -y ibus libibus-1.0-dev librime-dev librime-data \
        libnotify-dev cmake gcc pkg-config python3
      ;;
    dnf)
      sudo dnf install -y ibus ibus-devel librime librime-devel rime-data \
        libnotify-devel cmake gcc pkg-config python3
      ;;
    pacman)
      sudo pacman -S --needed --noconfirm ibus librime rime-data \
        libnotify cmake gcc pkg-config python
      ;;
    *)
      warn "Unknown package manager. Please install dependencies manually:"
      echo "  - ibus + ibus-devel"
      echo "  - librime + librime-dev + rime-data"
      echo "  - libnotify + libnotify-devel"
      echo "  - cmake, gcc, pkg-config, python3"
      ;;
  esac
}

check_pkg() {
  pkg-config --exists "$1" 2>/dev/null && info "$1 found" || { warn "$1 missing"; return 1; }
}

if check_pkg ibus-1.0 && check_pkg librime && check_pkg libnotify; then
  info "All dev packages present"
else
  install_pkgs
fi

# ── 3. init submodules (if applicable) ────────────────────────

header "Checking submodules"
if [ -f .gitmodules ]; then
  info "Git repo detected, initializing submodules..."
  git submodule update --init --recursive 2>/dev/null || warn "submodule init failed (ignored, using system packages)"
else
  info "Not a git repo, skipping submodule init"
fi

# ── 4. build and install ──────────────────────────────────────

header "Building qiwo-ibus"

# Install prefix
PREFIX="${PREFIX:-/usr}"

mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

header "Installing"
sudo make install

header "Restarting IBus"
ibus restart 2>/dev/null || warn "Could not restart ibus. Run 'ibus restart' manually."

header "Setup complete"
echo ""
echo "  IBus 组件: ibus-setup"
echo "  同步脚本:  $PREFIX/share/qiwo/qiwo_sync.py"
echo "  引擎路径:  $PREFIX/libexec/qiwo/ibus-engine-qiwo"
echo ""
echo "  WebDAV 配置方法:"
echo "    export QIWO_WEBDAV_URL=\"https://dav.example.com/qiwo-rime-sync\""
echo "    export QIWO_WEBDAV_USERNAME=\"username\""
echo "    export QIWO_WEBDAV_PASSWORD=\"password\""
echo ""
echo "  添加以上 export 到 ~/.bashrc 或 ~/.profile 中。"
