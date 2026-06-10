#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
help_text="$("$root/install-linux.sh" --help)"

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
