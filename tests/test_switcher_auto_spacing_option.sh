#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

python3 - "$root" <<'PY'
import sys
from pathlib import Path

root = Path(sys.argv[1])


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    sys.exit(1)


def require_contains(text: str, needle: str, label: str) -> None:
    if needle not in text:
        fail(f"{label} missing: {needle}")


def require_not_contains(text: str, needle: str, label: str) -> None:
    if needle in text:
        fail(f"{label} should not contain: {needle}")


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


default_config_source = read(root / "qiwo_rime_default_config.c")
sync_frost_source = read(root / "qiwo-sync-core" / "qiwo-sync" / "src" / "frost_init.rs")
settings_header = read(root / "rime_settings.h")
settings_source = read(root / "rime_settings.c")
engine_source = read(root / "rime_engine.c")
cmake_source = read(root / "CMakeLists.txt")

for source, label in [
    (default_config_source, "Linux default config"),
    (sync_frost_source, "sync-core frost init"),
]:
    require_contains(source, "switcher/hotkeys/@next: F4", label)
    require_contains(source, "switcher/save_options/@next: auto_commit_spacing", label)
    require_contains(source, "rime_frost.custom.yaml", label)
    require_contains(source, "switches/@next", label)
    require_contains(source, "auto_commit_spacing", label)
    require_contains(source, "关闭中英数字自动空格", label)
    require_contains(source, "开启中英数字自动空格", label)

require_not_contains(
    default_config_source,
    "rime-frost/default.yaml",
    "Linux default config must not patch bundled rime-frost in place",
)
require_not_contains(
    sync_frost_source,
    "switcher/save_options does not include auto_commit_spacing",
    "sync-core must not test or require modified rime-frost source schemas",
)

if "ibus_rime_get_initial_auto_commit_spacing_option" not in settings_header:
    fail("rime_settings.h does not declare the auto_commit_spacing option helper")
if "user_config_open" not in settings_source:
    fail("rime_settings.c does not use Rime user_config_open for user.yaml")
if "var/option/auto_commit_spacing" not in settings_source:
    fail("rime_settings.c does not read the switcher-saved user option")
if "input/auto_commit_spacing" not in settings_source:
    fail("rime_settings.c does not preserve input/auto_commit_spacing fallback")
if "auto_commit_spacing" not in engine_source or "set_option" not in engine_source:
    fail("rime_engine.c does not seed auto_commit_spacing on Rime sessions")
if "get_option" not in engine_source or '"auto_commit_spacing"' not in engine_source:
    fail("rime_engine.c does not read live auto_commit_spacing before commit")
if "g_ibus_rime_settings.auto_commit_spacing_enabled)" in engine_source:
    fail("rime_engine.c still formats commits from the static global setting")
if "switcher_auto_spacing_option" not in cmake_source:
    fail("CMakeLists.txt does not register the switcher auto spacing smoke test")
PY
