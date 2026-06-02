#!/usr/bin/env python3
"""Qiwo WebDAV Sync Tool for Linux (ibus-rime).

Usage:
  python3 qiwo_sync.py sync          --rime-user-dir <dir> --remote-url <url> [options]
  python3 qiwo_sync.py push          --rime-user-dir <dir> --remote-url <url> [options]
  python3 qiwo_sync.py pull          --rime-user-dir <dir> --remote-url <url> [options]
  python3 qiwo_sync.py sync-user-dict --rime-user-dir <dir> --remote-url <url> [options]
  python3 qiwo_sync.py init-frost    --rime-user-dir <dir> --frost-dir <dir> [options]

Options:
  --frontend <name>      Frontend name (default: ibus-rime)
  --username <name>      WebDAV username
  --password <pw>        WebDAV password (prefer --password-env)
  --password-env <var>   Environment variable containing password
  --device-id <id>       Device identifier
  --json                 Output result as JSON
  --dry-run              Simulate without writing
"""

import argparse, hashlib, json, os, shutil, sys, urllib.request
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import quote
from xml.etree import ElementTree as ET

VERSION = 1

# ── Constants (aligned with qiwo-sync-core SyncConstants) ─────────

STATE_DIR = ".qiwo-sync"
BACKUP_DIR = "backups"
MANIFEST_FILE = "manifest.json"
REMOTE_MANIFEST_FILE = ".qiwo-sync-manifest.json"
FROST_SCHEMA_FILE = "rime_frost.schema.yaml"
DEFAULT_CUSTOM_YAML = "default.custom.yaml"

# ── File Selector (aligned with C# FileSelector.ShouldSync) ──────

INCLUDED_EXACT = {"custom_phrase.txt", "symbols.yaml"}
INCLUDED_EXTENSIONS = (".custom.yaml", ".schema.yaml", ".dict.yaml")
INCLUDED_DIRS = ("opencc/", "lua/", "sync/")
EXCLUDED_DIRS = (".git/", ".qiwo-sync/", "build/")
EXCLUDED_EXTENSIONS = (".bin",)
EXCLUDED_SUFFIXES = (".table.bin", ".reverse.bin", ".userdb")


def should_sync(relpath):
    """判断文件是否应该同步，与 C# FileSelector.ShouldSync 一致。"""
    p = relpath.replace("\\", "/").lstrip("/")
    if not p:
        return False
    lower = p.lower()

    # 排除目录
    for d in EXCLUDED_DIRS:
        if lower.startswith(d):
            return False

    # 排除包含 .userdb 的路径段
    if any(seg.endswith(".userdb") for seg in lower.split("/")):
        return False

    # 排除后缀和扩展名
    for s in EXCLUDED_SUFFIXES:
        if lower.endswith(s):
            return False
    for s in EXCLUDED_EXTENSIONS:
        if lower.endswith(s):
            return False

    # 精确文件名匹配
    name = p.rsplit("/", 1)[-1]
    if name.lower() in INCLUDED_EXACT:
        return True

    # 扩展名匹配
    if lower.endswith(INCLUDED_EXTENSIONS):
        return True

    # 目录匹配
    for d in INCLUDED_DIRS:
        if lower.startswith(d):
            return True

    return False


def is_frost_resource(relpath):
    """判断文件是否应该从 frost 目录复制（与 C# FrostInitializer.IsFrostResource 一致）。"""
    if should_sync(relpath):
        return True
    lower = relpath.lower().replace("\\", "/").lstrip("/")
    filename = lower.rsplit("/", 1)[-1] if "/" in lower else lower
    if lower.endswith(".yaml"):
        return True
    if filename == "installation.yaml":
        return True
    for prefix in ("cn_dicts/", "en_dicts/", "others/"):
        if lower.startswith(prefix):
            return True
    return False


# ── SHA256 ────────────────────────────────────────────────────────

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


# ── Installation Helper (aligned with C# InstallationHelper) ──────

SYNC_DIR_NAME = "sync"


def make_safe_id(device_id):
    """将设备 ID 转换为文件系统安全的目录名。"""
    return device_id.replace(" ", "-").replace(":", "-").replace("\\", "-").replace("/", "-").lower()


def ensure_installation(user_dir, device_id):
    """确保 installation.yaml 存在且包含正确的 installation_id 和 sync_dir。
    与 C# InstallationHelper.Ensure 一致。"""
    file_path = os.path.join(user_dir, "installation.yaml")
    safe_id = make_safe_id(device_id)

    if os.path.exists(file_path):
        with open(file_path, "r", encoding="utf-8") as f:
            existing = f.read().rstrip()
        needs_update = False
        updated = existing

        if "sync_dir:" not in updated:
            updated += '\nsync_dir: "sync"\n'
            needs_update = True
        if "installation_id:" not in updated:
            updated += f'\ninstallation_id: "{safe_id}"\n'
            needs_update = True

        if needs_update:
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(updated)
        return

    # 新建
    yaml = (
        'distribution: "Qiwo"\n'
        'distribution_version: "1.0"\n'
        f'installation_id: "{safe_id}"\n'
        f'sync_dir: "{SYNC_DIR_NAME}"\n'
    )
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(yaml)


def ensure_sync_export_dir(user_dir, device_id):
    """确保 sync/{deviceId}/ 目录存在。"""
    d = os.path.join(user_dir, SYNC_DIR_NAME, make_safe_id(device_id))
    os.makedirs(d, exist_ok=True)
    return d


# ── Manifest ──────────────────────────────────────────────────────

def load_manifest(path):
    try:
        with open(path, "r") as f:
            return json.load(f)
    except Exception:
        return {"version": VERSION, "device_id": "", "frontend": "ibus-rime",
                "updated_at": "", "files": {}}


def save_manifest(path, m):
    m["updated_at"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    m["version"] = VERSION
    with open(path, "w") as f:
        json.dump(m, f, indent=2)


# ── WebDAV Client ─────────────────────────────────────────────────

class WebDavClient:
    def __init__(self, base_url, username="", password=""):
        self.base = base_url.rstrip("/")
        self.auth = None
        if username or password:
            from base64 import b64encode
            creds = b64encode(f"{username}:{password}".encode()).decode()
            self.auth = f"Basic {creds}"

    def _req(self, method, path="", data=None, headers=None):
        url = (f"{self.base}/{quote(path.lstrip('/'), safe='/')}"
               if path else self.base)
        req = urllib.request.Request(url, data=data, method=method)
        req.add_header("User-Agent", "QiwoIbus/1.0")
        if self.auth:
            req.add_header("Authorization", self.auth)
        if headers:
            for k, v in headers.items():
                req.add_header(k, v)
        try:
            resp = urllib.request.urlopen(req, timeout=30)
            return resp.status, resp.read()
        except urllib.error.HTTPError as e:
            return e.code, e.read()
        except Exception as e:
            return 0, str(e).encode()

    def ensure_root(self):
        """确保远端根目录存在（MKCOL，与 C# EnsureRootAsync 一致）。"""
        status, _ = self._req("MKCOL", "")
        # 201=created, 405=already exists, 200/204=ok
        if status in (200, 201, 204, 405):
            return
        # 逐级创建
        parts = self.base.rstrip("/").split("/")[3:]  # skip https://host
        cur = ""
        for part in parts:
            if not part:
                continue
            cur = f"{cur}/{part}" if cur else part
            self._req("MKCOL", cur)

    def download(self, path):
        status, data = self._req("GET", path)
        if status == 200:
            return data
        if status == 404:
            return None
        raise Exception(f"Download failed: HTTP {status} for {path}")

    def upload(self, path, data):
        self._ensure_dir(os.path.dirname(path))
        status, _ = self._req("PUT", path, data)
        if status not in (200, 201, 204):
            raise Exception(f"Upload failed: HTTP {status} for {path}")

    def _ensure_dir(self, path):
        if not path or path == ".":
            return
        parts = path.replace("\\", "/").strip("/").split("/")
        cur = ""
        for p in parts:
            cur = f"{cur}/{p}" if cur else p
            self._req("MKCOL", cur)

    def download_manifest(self):
        try:
            data = self.download(REMOTE_MANIFEST_FILE)
            return data
        except Exception:
            return None

    def upload_manifest(self, data):
        self.upload(REMOTE_MANIFEST_FILE, data)


# ── Sync Engine ───────────────────────────────────────────────────

def scan_files(user_dir):
    """扫描本地文件（仅通过 should_sync 过滤，与 C# LocalFileStore.Scan 一致）。"""
    files = {}
    for root, dirs, filenames in os.walk(user_dir):
        # os.walk 进入子目录，由 should_sync 负责过滤
        for fn in filenames:
            fp = os.path.join(root, fn)
            rp = os.path.relpath(fp, user_dir).replace("\\", "/")
            if not should_sync(rp):
                continue
            st = os.stat(fp)
            files[rp] = {
                "relativePath": rp, "size": st.st_size,
                "sha256": sha256_file(fp),
                "lastWriteUtc": datetime.fromtimestamp(
                    st.st_mtime, tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
    return files


def _prepare_dirs(user_dir):
    """创建 .qiwo-sync 状态目录。"""
    sync_dir = os.path.join(user_dir, STATE_DIR)
    os.makedirs(sync_dir, exist_ok=True)
    backup_dir = os.path.join(sync_dir, BACKUP_DIR)
    local_mf_path = os.path.join(sync_dir, MANIFEST_FILE)
    return sync_dir, backup_dir, local_mf_path


def _do_three_way_merge(user_dir, client, device_id, frontend,
                         local_files, remote_files, prev_files,
                         dry_run=False, label="Sync"):
    """三路合并核心逻辑，与 C# SyncEngine.SyncAsync 一致。"""
    _, backup_dir, local_mf_path = _prepare_dirs(user_dir)

    all_paths = set(local_files.keys()) | set(remote_files.keys()) | set(prev_files.keys())
    new_files = {}
    stats = {"uploaded": 0, "downloaded": 0, "skipped": 0, "conflicts": 0}

    for path in sorted(all_paths):
        if not should_sync(path):
            continue
        loc = local_files.get(path)
        rem = remote_files.get(path)
        prv = prev_files.get(path)

        try:
            # 文件相同 → 跳过
            if loc and rem and loc["sha256"] == rem["sha256"]:
                new_files[path] = loc
                stats["skipped"] += 1
                continue

            lc = loc and (not prv or loc["sha256"] != prv.get("sha256", ""))
            rc = rem and (not prv or rem["sha256"] != prv.get("sha256", ""))

            # 仅本地有
            if loc and not rem:
                if not dry_run:
                    _do_upload(client, user_dir, path)
                new_files[path] = loc
                stats["uploaded"] += 1
            # 仅远端有
            elif not loc and rem:
                if not dry_run:
                    entry = _do_download(client, user_dir, path)
                    new_files[path] = entry
                else:
                    new_files[path] = rem
                stats["downloaded"] += 1
            # 仅本地变更
            elif loc and rem and lc and not rc:
                if not dry_run:
                    _do_upload(client, user_dir, path)
                new_files[path] = loc
                stats["uploaded"] += 1
            # 仅远端变更
            elif loc and rem and not lc and rc:
                if not dry_run:
                    entry = _do_download(client, user_dir, path)
                    new_files[path] = entry
                else:
                    new_files[path] = rem
                stats["downloaded"] += 1
            # 双方都变更 → 冲突：备份本地，保留远端
            elif loc and rem and lc and rc:
                if not dry_run:
                    _do_backup(user_dir, backup_dir, path)
                    entry = _do_download(client, user_dir, path)
                    new_files[path] = entry
                else:
                    new_files[path] = rem
                stats["downloaded"] += 1
                stats["conflicts"] += 1
            # 都未变更 → 以时间戳为准
            elif loc and rem:
                if loc["lastWriteUtc"] >= rem["lastWriteUtc"]:
                    if not dry_run:
                        _do_upload(client, user_dir, path)
                    new_files[path] = loc
                    stats["uploaded"] += 1
                else:
                    if not dry_run:
                        entry = _do_download(client, user_dir, path)
                        new_files[path] = entry
                    else:
                        new_files[path] = rem
                    stats["downloaded"] += 1
        except Exception as e:
            print(f"Error syncing {path}: {e}", file=sys.stderr)

    # 写入清单
    if not dry_run:
        manifest = {"version": VERSION, "device_id": device_id,
                     "frontend": frontend, "updated_at": "", "files": new_files}
        save_manifest(local_mf_path, manifest)
        client.upload_manifest(json.dumps(manifest).encode())

    return stats


def sync(user_dir, client, device_id, frontend, dry_run=False):
    """双向同步（sync 模式）。"""
    _, _, local_mf_path = _prepare_dirs(user_dir)
    local_files = scan_files(user_dir)
    prev_manifest = load_manifest(local_mf_path)
    prev_files = prev_manifest.get("files", {})

    remote_raw = client.download_manifest()
    remote_manifest = json.loads(remote_raw) if remote_raw else {"files": {}}
    remote_files = remote_manifest.get("files", {})

    return _do_three_way_merge(
        user_dir, client, device_id, frontend,
        local_files, remote_files, prev_files,
        dry_run=dry_run, label="Sync")


def sync_user_dict(user_dir, client, device_id, frontend, dry_run=False):
    """仅同步用户词库（sync/ 目录），与 C# SyncUserDictAsync 一致。"""
    _, _, local_mf_path = _prepare_dirs(user_dir)
    local_files = scan_files(user_dir)
    prev_manifest = load_manifest(local_mf_path)
    prev_files = prev_manifest.get("files", {})

    remote_raw = client.download_manifest()
    remote_manifest = json.loads(remote_raw) if remote_raw else {"files": {}}
    remote_files = remote_manifest.get("files", {})

    # 过滤：仅保留 sync/ 目录下的文件
    local_dict_files = {k: v for k, v in local_files.items() if k.startswith("sync/")}
    remote_dict_files = {k: v for k, v in remote_files.items() if k.startswith("sync/")}

    return _do_three_way_merge(
        user_dir, client, device_id, frontend,
        local_dict_files, remote_dict_files, prev_files,
        dry_run=dry_run, label="Dict sync")


def push(user_dir, client, device_id, frontend, dry_run=False):
    """推送：上传所有本地文件到远端。"""
    _, _, local_mf_path = _prepare_dirs(user_dir)
    local_files = scan_files(user_dir)
    uploaded = 0

    for path in sorted(local_files.keys()):
        if not dry_run:
            _do_upload(client, user_dir, path)
        uploaded += 1

    if not dry_run:
        manifest = {"version": VERSION, "device_id": device_id,
                     "frontend": frontend, "updated_at": "", "files": local_files}
        save_manifest(local_mf_path, manifest)
        client.upload_manifest(json.dumps(manifest).encode())

    return {"uploaded": uploaded, "downloaded": 0, "skipped": 0, "conflicts": 0}


def pull(user_dir, client, device_id, frontend, dry_run=False):
    """拉取：从远端下载所有文件到本地。"""
    _, _, local_mf_path = _prepare_dirs(user_dir)
    remote_raw = client.download_manifest()
    remote_manifest = json.loads(remote_raw) if remote_raw else {"files": {}}
    remote_files = remote_manifest.get("files", {})

    downloaded = 0
    skipped = 0

    for path in sorted(remote_files.keys()):
        if not should_sync(path):
            skipped += 1
            continue
        if not dry_run:
            _do_download(client, user_dir, path)
        downloaded += 1

    if not dry_run:
        local_files = scan_files(user_dir)
        manifest = {"version": VERSION, "device_id": device_id,
                     "frontend": frontend, "updated_at": "", "files": local_files}
        save_manifest(local_mf_path, manifest)

    return {"uploaded": 0, "downloaded": downloaded, "skipped": skipped, "conflicts": 0}


def init_frost(user_dir, frost_dir, dry_run=False):
    """初始化 rime-frost：将方案文件复制到 Rime 用户目录。
    与 C# FrostInitializer.InitializeAsync 一致。"""
    frost_dir = os.path.abspath(frost_dir)
    if not os.path.isdir(frost_dir):
        print(f"Error: frost directory not found: {frost_dir}", file=sys.stderr)
        sys.exit(2)

    sentinel = os.path.join(user_dir, FROST_SCHEMA_FILE)
    if os.path.exists(sentinel):
        print("rime-frost already initialized.")
        return {"downloaded": 0, "skipped": 0, "conflicts": 0}

    copied = 0
    skipped = 0

    for root, dirs, filenames in os.walk(frost_dir):
        # 排除 .git 目录
        dirs[:] = [d for d in dirs if d != ".git"]

        for fn in filenames:
            src = os.path.join(root, fn)
            rp = os.path.relpath(src, frost_dir).replace("\\", "/")

            if not is_frost_resource(rp):
                continue

            dst = os.path.join(user_dir, rp)
            if os.path.exists(dst):
                skipped += 1
                continue

            if not dry_run:
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(src, dst)
            copied += 1

    # 确保 default.custom.yaml 存在
    default_custom = os.path.join(user_dir, DEFAULT_CUSTOM_YAML)
    if not os.path.exists(default_custom) or os.path.getsize(default_custom) == 0:
        if not dry_run:
            with open(default_custom, "w") as f:
                f.write("patch:\n  schema_list:\n    - schema: rime_frost\n")

    return {"downloaded": copied, "skipped": skipped, "conflicts": 0}


# ── Helpers ───────────────────────────────────────────────────────

def _do_upload(client, user_dir, path):
    fp = os.path.join(user_dir, path)
    with open(fp, "rb") as f:
        client.upload(path, f.read())


def _do_download(client, user_dir, path):
    data = client.download(path)
    if data is None:
        raise Exception(f"Remote file not found: {path}")
    fp = os.path.join(user_dir, path)
    os.makedirs(os.path.dirname(fp), exist_ok=True)
    with open(fp, "wb") as f:
        f.write(data)
    return {"relativePath": path, "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "lastWriteUtc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}


def _do_backup(user_dir, backup_dir, path):
    src = os.path.join(user_dir, path)
    if not os.path.exists(src):
        return
    ts = datetime.now().strftime("%Y%m%d%H%M%S")
    dst = os.path.join(backup_dir, ts, path)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)


# ── Main ──────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="Qiwo WebDAV Sync Tool")
    p.add_argument("mode", choices=["sync", "push", "pull", "sync-user-dict", "init-frost"])
    p.add_argument("--rime-user-dir", help="Rime user data directory")
    p.add_argument("--remote-url", help="WebDAV server URL")
    p.add_argument("--username", default="")
    p.add_argument("--password", default="")
    p.add_argument("--password-env", default="QIWO_WEBDAV_PASSWORD")
    p.add_argument("--device-id", default="")
    p.add_argument("--frontend", default="ibus-rime")
    p.add_argument("--frost-dir", help="rime-frost repository directory (for init-frost)")
    p.add_argument("--json", action="store_true")
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args()

    # init-frost 不需要 remote-url
    if args.mode == "init-frost":
        if not args.rime_user_dir or not args.frost_dir:
            p.error("--rime-user-dir and --frost-dir are required for init-frost")
        stats = init_frost(args.rime_user_dir, args.frost_dir, dry_run=args.dry_run)
        if args.json:
            print(json.dumps(stats))
        else:
            print(f"Frost init: copied={stats['downloaded']} skipped={stats['skipped']}")
        return 0

    # 其他模式需要 remote-url
    if not args.remote_url or not args.rime_user_dir:
        p.error("--rime-user-dir and --remote-url are required for sync/push/pull/sync-user-dict")

    password = args.password
    if args.password_env and not password:
        password = os.environ.get(args.password_env, "")

    device_id = args.device_id or os.uname().nodename

    # 确保 installation.yaml 配置正确
    ensure_installation(args.rime_user_dir, device_id)

    client = WebDavClient(args.remote_url, args.username, password)

    if not args.dry_run and args.mode != "init-frost":
        client.ensure_root()

    if args.mode == "sync":
        stats = sync(args.rime_user_dir, client, device_id, args.frontend,
                     dry_run=args.dry_run)
    elif args.mode == "sync-user-dict":
        stats = sync_user_dict(args.rime_user_dir, client, device_id, args.frontend,
                               dry_run=args.dry_run)
    elif args.mode == "push":
        stats = push(args.rime_user_dir, client, device_id, args.frontend,
                     dry_run=args.dry_run)
    elif args.mode == "pull":
        stats = pull(args.rime_user_dir, client, device_id, args.frontend,
                     dry_run=args.dry_run)
    else:
        p.error(f"Unknown mode: {args.mode}")

    if args.json:
        print(json.dumps(stats))
    else:
        print(f"{args.mode}: uploaded={stats['uploaded']} downloaded={stats['downloaded']} "
              f"skipped={stats['skipped']} conflicts={stats['conflicts']}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
