#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

python3 - "$root" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])

SCHEMA_FILES = [
    "rime_frost.schema.yaml",
    "rime_frost_double_pinyin.schema.yaml",
    "rime_frost_double_pinyin_mspy.schema.yaml",
    "rime_frost_double_pinyin_sogou.schema.yaml",
    "rime_frost_double_pinyin_flypy.schema.yaml",
    "rime_frost_double_pinyin_abc.schema.yaml",
    "rime_frost_double_pinyin_ziguang.schema.yaml",
    "rime_frost_t9.schema.yaml",
    "rime_frost_wubi86.schema.yaml",
    "rime_frost_moqi_single_xh.schema.yaml",
]

EXPECTED_STATES = ["关闭中英数字自动空格", "开启中英数字自动空格"]


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    sys.exit(1)


def strip_inline_comment(line: str) -> str:
    in_single = False
    in_double = False
    for index, char in enumerate(line):
        if char == "'" and not in_double:
            in_single = not in_single
        elif char == '"' and not in_single:
            in_double = not in_double
        elif char == "#" and not in_single and not in_double:
            return line[:index]
    return line


def indent_of(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def top_level_block(lines, key):
    start = None
    for index, line in enumerate(lines):
        if re.match(rf"^{re.escape(key)}:\s*(?:#.*)?$", line):
            start = index
            break
    if start is None:
        return []

    end = len(lines)
    for index in range(start + 1, len(lines)):
        line = lines[index]
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if indent_of(line) == 0 and not line.startswith(" "):
            end = index
            break
    return lines[start:end]


def list_items_in_block(block, key):
    for index, line in enumerate(block):
        match = re.match(r"^(\s*)" + re.escape(key) + r":", line)
        if not match:
            continue
        list_indent = len(match.group(1))
        items = []
        for item_line in block[index + 1:]:
            if not item_line.strip() or item_line.lstrip().startswith("#"):
                continue
            if indent_of(item_line) <= list_indent:
                break
            item_match = re.match(r"^\s*-\s*([^#\s]+)", item_line)
            if item_match:
                items.append(item_match.group(1))
        return items
    return []


def read_lines(path: Path):
    return path.read_text(encoding="utf-8").splitlines()


def require_default_save_option() -> None:
    lines = read_lines(root / "rime-frost" / "default.yaml")
    switcher = top_level_block(lines, "switcher")
    if not switcher:
        fail("default.yaml is missing switcher block")
    hotkeys = list_items_in_block(switcher, "hotkeys")
    if "Control+grave" not in hotkeys or "F4" not in hotkeys:
        fail("default.yaml switcher/hotkeys must include Control+grave and F4")
    save_options = list_items_in_block(switcher, "save_options")
    if "auto_commit_spacing" not in save_options:
        fail("default.yaml switcher/save_options does not include auto_commit_spacing")


def parse_switches(path: Path):
    lines = read_lines(path)
    switch_block = top_level_block(lines, "switches")
    switches = {}
    current_name = None
    current_indent = None
    current_lines = []

    for line in switch_block[1:]:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        match = re.match(r"^(\s*)-\s+name:\s*([^#\s]+)", line)
        if match:
            if current_name is not None:
                switches[current_name] = current_lines
            current_indent = len(match.group(1))
            current_name = match.group(2)
            current_lines = [line]
            continue
        if current_name is not None:
            if indent_of(line) <= current_indent and line.lstrip().startswith("-"):
                switches[current_name] = current_lines
                current_name = None
                current_lines = []
            else:
                current_lines.append(line)

    if current_name is not None:
        switches[current_name] = current_lines
    return switches


def switch_has_expected_states(block_lines) -> bool:
    for line in block_lines:
        line = strip_inline_comment(line).strip()
        match = re.match(r"^states:\s*\[(.*)\]\s*$", line)
        if not match:
            continue
        states = [state.strip() for state in match.group(1).split(",")]
        return states == EXPECTED_STATES
    return False


def require_schema_switch(path: Path, base_switch_ok: bool) -> None:
    text = path.read_text(encoding="utf-8")
    switches = parse_switches(path)
    block = switches.get("auto_commit_spacing")
    if block is None and "__include: rime_frost.schema.yaml:/" in text and base_switch_ok:
        return
    if block is None:
        fail(f"{path.name} is missing switches entry name: auto_commit_spacing")
    if not switch_has_expected_states(block):
        fail(f"{path.name} auto_commit_spacing states are not {EXPECTED_STATES}")
    if any(re.match(r"^\s*reset\s*:", line) for line in block):
        fail(f"{path.name} auto_commit_spacing switch must not define reset")


def require_schema_switches() -> None:
    base_path = root / "rime-frost" / "rime_frost.schema.yaml"
    base_switch = parse_switches(base_path).get("auto_commit_spacing")
    base_switch_ok = bool(base_switch) and switch_has_expected_states(base_switch)
    for schema_file in SCHEMA_FILES:
        require_schema_switch(root / "rime-frost" / schema_file, base_switch_ok)


def require_source_integration() -> None:
    settings_header = (root / "rime_settings.h").read_text(encoding="utf-8")
    settings_source = (root / "rime_settings.c").read_text(encoding="utf-8")
    engine_source = (root / "rime_engine.c").read_text(encoding="utf-8")
    cmake_source = (root / "CMakeLists.txt").read_text(encoding="utf-8")

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


require_default_save_option()
require_schema_switches()
require_source_integration()
PY
