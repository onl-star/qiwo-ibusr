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

if ! grep -q 'git -C "$script_dir" submodule update --init --recursive qiwo-sync-core' <<<"$script_text"; then
  echo "install-linux.sh does not auto-initialize the bundled qiwo-sync-core submodule" >&2
  exit 1
fi

if ! grep -q 'qiwo-sync-core submodule missing, initializing' <<<"$script_text"; then
  echo "install-linux.sh does not report automatic qiwo-sync-core submodule initialization" >&2
  exit 1
fi
