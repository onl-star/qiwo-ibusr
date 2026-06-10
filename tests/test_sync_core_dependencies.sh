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
