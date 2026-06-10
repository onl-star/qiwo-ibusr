#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if grep -Eiq '\bpython3?\b' "$root/Makefile" "$root/INSTALL.md"; then
  echo "Python is still documented as a Linux sync runtime dependency" >&2
  exit 1
fi

grep -Eiq '\b(cargo|rust)\b' "$root/Makefile" || {
  echo "Makefile does not document Rust/Cargo dependency for source builds" >&2
  exit 1
}

grep -Eiq '\b(Cargo|Rust)\b' "$root/INSTALL.md" || {
  echo "INSTALL.md does not document Rust/Cargo dependency for source builds" >&2
  exit 1
}

grep -Fq 'for pkg in ibus-1.0 rime libnotify gtk+-3.0 libsecret-1' "$root/Makefile" || {
  echo "Makefile should check the rime pkg-config module, not a non-portable librime module" >&2
  exit 1
}

if grep -q 'submodule update --init --recursive 2>/dev/null || true' "$root/Makefile"; then
  echo "Makefile setup should not initialize every submodule during Linux installs" >&2
  exit 1
fi

grep -Fq 'EXISTS "${_RIME_DATA_DIR}/default.yaml"' "$root/cmake/FindRimeData.cmake" || {
  echo "FindRimeData.cmake should require default.yaml in the selected Rime data directory" >&2
  exit 1
}
