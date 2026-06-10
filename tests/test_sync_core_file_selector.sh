#!/usr/bin/env bash
set -euo pipefail

sync_core_dir="${QIWO_SYNC_CORE_DIR:-}"
if [[ -z "$sync_core_dir" ]]; then
  candidate="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)/qiwo-sync-core"
  if [[ -f "$candidate/Cargo.toml" ]]; then
    sync_core_dir="$candidate"
  fi
fi

if [[ -z "$sync_core_dir" || ! -f "$sync_core_dir/Cargo.toml" ]]; then
  echo "qiwo-sync-core source tree not available; skipping shared file selector test"
  exit 0
fi

cargo test --manifest-path "$sync_core_dir/Cargo.toml" -p qiwo-sync file_selector
