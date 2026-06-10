#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
help_text="$("$root/install-linux.sh" --help)"
script_text="$(cat "$root/install-linux.sh")"

for expected in \
  "--sync-bin" \
  "--sync-core-dir" \
  "QIWO_RIME_SYNC_BIN" \
  "QIWO_SYNC_CORE_DIR"; do
  if ! grep -q -- "$expected" <<<"$help_text"; then
    echo "install-linux.sh --help does not mention $expected" >&2
    exit 1
  fi
done

for expected in \
  'scripts/setup-linux-deps.sh' \
  '$HOME/.cargo/bin' \
  '--need-cargo' \
  '--need-git' \
  '--no-install'; do
  if ! grep -q -- "$expected" <<<"$script_text"; then
    echo "install-linux.sh does not wire dependency setup option: $expected" >&2
    exit 1
  fi
done

if ! grep -q 'git -C "$script_dir" submodule update --init --recursive "$name"' <<<"$script_text"; then
  echo "install-linux.sh does not auto-initialize required submodules by name" >&2
  exit 1
fi

if ! grep -q '"rime-frost"' <<<"$script_text"; then
  echo "install-linux.sh does not require the rime-frost submodule" >&2
  exit 1
fi

if ! grep -q '"qiwo-sync-core"' <<<"$script_text"; then
  echo "install-linux.sh does not require the bundled qiwo-sync-core submodule" >&2
  exit 1
fi

if ! grep -q 'git submodule update --init --recursive rime-frost' <<<"$script_text"; then
  echo "install-linux.sh does not give a targeted rime-frost submodule hint" >&2
  exit 1
fi

if ! grep -q 'git submodule update --init --recursive qiwo-sync-core' <<<"$script_text"; then
  echo "install-linux.sh does not give a targeted qiwo-sync-core submodule hint" >&2
  exit 1
fi

if grep -q 'git submodule update --init --recursive librime' <<<"$script_text"; then
  echo "install-linux.sh should not initialize the librime submodule for Linux installs" >&2
  exit 1
fi

if grep -q 'git submodule update --init --recursive plum' <<<"$script_text"; then
  echo "install-linux.sh should not initialize the plum submodule for Linux installs" >&2
  exit 1
fi

for expected in \
  'pkg-config --variable=pkgdatadir rime' \
  'dpkg-query -L librime-data rime-data' \
  'rpm -ql rime-data librime-data librime' \
  'pacman -Ql rime-data librime'; do
  if ! grep -q -- "$expected" <<<"$script_text"; then
    echo "install-linux.sh does not include portable Rime data detection for: $expected" >&2
    exit 1
  fi
done

if ! grep -q 'default.yaml' <<<"$script_text"; then
  echo "install-linux.sh should verify that Rime data directories contain default.yaml" >&2
  exit 1
fi
