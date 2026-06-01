#!/usr/bin/env python3
"""Qiwo WebDAV Sync Tool for Linux (ibus-rime).

Usage:
  python3 qiwo_sync.py sync --rime-user-dir <dir> --remote-url <url> [options]
  python3 qiwo_sync.py push  --rime-user-dir <dir> --remote-url <url> [options]
  python3 qiwo_sync.py pull  --rime-user-dir <dir> --remote-url <url> [options]

Options:
  --username <name>       WebDAV username
  --password <pw>         WebDAV password (prefer --password-env)
  --password-env <var>    Environment variable containing password
  --device-id <id>        Device identifier
  --json                  Output result as JSON
"""

import argparse, hashlib, json, os, shutil, sys, urllib.request
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import quote
from xml.etree import ElementTree as ET

VERSION = 1

# ── File Selector ──────────────────────────────────────────────

EXACT_FILES = {"custom_phrase.txt", "symbols.yaml"}
EXTENSIONS = (".custom.yaml", ".schema.yaml", ".dict.yaml")
DIRS = ("opencc/", "lua/")
EXCLUDE_DIRS = {".git", ".qiwo-sync", "build"}

def should_sync(relpath):
    p = relpath.replace("\\", "/").lstrip("/")
    if not p: return False
    name = p.rsplit("/", 1)[-1]
    for d in EXCLUDE_DIRS:
        if p == d or p.startswith(d + "/"): return False
    for s in (".bin", ".userdb"):
        if p.endswith(s) or ("/" + s.lstrip(".") + "/") in p: return False
    if name in EXACT_FILES: return True
    if name.endswith(EXTENSIONS): return True
    for d in DIRS:
        if p.startswith(d): return True
    return False

# ── SHA256 ─────────────────────────────────────────────────────

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()

# ── Manifest ───────────────────────────────────────────────────

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

# ── WebDAV Client ──────────────────────────────────────────────

class WebDavClient:
    def __init__(self, base_url, username="", password=""):
        self.base = base_url.rstrip("/")
        self.auth = None
        if username or password:
            from base64 import b64encode
            creds = b64encode(f"{username}:{password}".encode()).decode()
            self.auth = f"Basic {creds}"

    def _req(self, method, path="", data=None, headers=None):
        url = f"{self.base}/{quote(path.lstrip('/'), safe='/')}" if path else self.base
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

    def propfind(self, path="", depth="1"):
        body = """<?xml version="1.0"?>
            <propfind xmlns="DAV:"><prop>
            <resourcetype/><getcontentlength/><getlastmodified/>
            </prop></propfind>"""
        status, data = self._req("PROPFIND", path, body.encode(),
                                 {"Depth": depth, "Content-Type": "application/xml"})
        if status not in (207,):
            return []
        ns = {"D": "DAV:"}
        root = ET.fromstring(data)
        entries = []
        for resp in root.findall(".//D:response", ns):
            href = (resp.findtext("D:href", "", ns) or "").rstrip("/")
            rel = href.split("/")[-1] if "/" in href else href
            if not rel: continue
            coll = resp.find(".//D:collection", ns) is not None
            length = resp.findtext(".//D:getcontentlength", "0", ns) or "0"
            modified = resp.findtext(".//D:getlastmodified", "", ns) or ""
            entries.append({"href": rel, "is_collection": coll,
                            "content_length": int(length), "last_modified": modified})
        return entries

    def download(self, path):
        status, data = self._req("GET", path)
        if status == 200: return data
        raise Exception(f"Download failed: HTTP {status}")

    def upload(self, path, data):
        self._ensure_dir(os.path.dirname(path))
        status, _ = self._req("PUT", path, data)
        if status not in (200, 201, 204):
            raise Exception(f"Upload failed: HTTP {status}")

    def _ensure_dir(self, path):
        if not path or path == ".": return
        parts = path.replace("\\", "/").strip("/").split("/")
        cur = ""
        for p in parts:
            cur = f"{cur}/{p}" if cur else p
            status, _ = self._req("MKCOL", cur)
            # 201=created, 405=exists, 409=conflict(exists)

    def download_manifest(self):
        try: return self.download(".qiwo-sync-manifest.json")
        except Exception: return b"{}"

    def upload_manifest(self, data):
        self.upload(".qiwo-sync-manifest.json", data)

# ── Sync Engine ────────────────────────────────────────────────

def scan_files(user_dir):
    files = {}
    for root, dirs, filenames in os.walk(user_dir):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        for fn in filenames:
            fp = os.path.join(root, fn)
            rp = os.path.relpath(fp, user_dir).replace("\\", "/")
            if not should_sync(rp): continue
            st = os.stat(fp)
            files[rp] = {
                "relativePath": rp, "size": st.st_size,
                "sha256": sha256_file(fp),
                "lastWriteUtc": datetime.fromtimestamp(
                    st.st_mtime, tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
    return files

def sync(user_dir, client, device_id, frontend):
    sync_dir = os.path.join(user_dir, ".qiwo-sync")
    os.makedirs(sync_dir, exist_ok=True)
    backup_dir = os.path.join(sync_dir, "backups")
    local_mf_path = os.path.join(sync_dir, "manifest.json")

    local_files = scan_files(user_dir)
    prev_manifest = load_manifest(local_mf_path)
    prev_files = prev_manifest.get("files", {})

    remote_raw = client.download_manifest()
    remote_manifest = json.loads(remote_raw) if remote_raw else {"files": {}}
    remote_files = remote_manifest.get("files", {})

    all_paths = set(local_files.keys()) | set(remote_files.keys()) | set(prev_files.keys())
    new_files = {}
    stats = {"uploaded": 0, "downloaded": 0, "skipped": 0, "conflicts": 0}

    for path in sorted(all_paths):
        if not should_sync(path): continue
        loc = local_files.get(path)
        rem = remote_files.get(path)
        prv = prev_files.get(path)

        try:
            if loc and rem and loc["sha256"] == rem["sha256"]:
                new_files[path] = loc; stats["skipped"] += 1
            elif loc and not rem:
                _upload(client, user_dir, path, loc)
                new_files[path] = loc; stats["uploaded"] += 1
            elif not loc and rem:
                entry = _download(client, user_dir, path)
                new_files[path] = entry; stats["downloaded"] += 1
            elif loc and rem:
                lc = not prv or loc["sha256"] != prv.get("sha256", "")
                rc = not prv or rem["sha256"] != prv.get("sha256", "")
                if lc and not rc:
                    _upload(client, user_dir, path, loc)
                    new_files[path] = loc; stats["uploaded"] += 1
                elif not lc and rc:
                    entry = _download(client, user_dir, path)
                    new_files[path] = entry; stats["downloaded"] += 1
                elif lc and rc:
                    _backup(user_dir, backup_dir, path)
                    entry = _download(client, user_dir, path)
                    new_files[path] = entry
                    stats["downloaded"] += 1; stats["conflicts"] += 1
                else:
                    if loc["lastWriteUtc"] > rem["lastWriteUtc"]:
                        _upload(client, user_dir, path, loc)
                        new_files[path] = loc; stats["uploaded"] += 1
                    else:
                        entry = _download(client, user_dir, path)
                        new_files[path] = entry; stats["downloaded"] += 1
        except Exception as e:
            print(f"Error syncing {path}: {e}", file=sys.stderr)

    manifest = {"version": VERSION, "device_id": device_id,
                "frontend": frontend, "updated_at": "", "files": new_files}
    save_manifest(local_mf_path, manifest)
    client.upload_manifest(json.dumps(manifest).encode())
    return stats

def _upload(client, user_dir, path, entry):
    fp = os.path.join(user_dir, path)
    with open(fp, "rb") as f:
        client.upload(path, f.read())

def _download(client, user_dir, path):
    data = client.download(path)
    fp = os.path.join(user_dir, path)
    os.makedirs(os.path.dirname(fp), exist_ok=True)
    with open(fp, "wb") as f:
        f.write(data)
    return {"relativePath": path, "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "lastWriteUtc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}

def _backup(user_dir, backup_dir, path):
    src = os.path.join(user_dir, path)
    if not os.path.exists(src): return
    ts = datetime.now().strftime("%Y%m%d%H%M%S")
    dst = os.path.join(backup_dir, ts, path)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)

# ── Main ───────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="Qiwo WebDAV Sync Tool")
    p.add_argument("mode", choices=["sync", "push", "pull"])
    p.add_argument("--rime-user-dir", required=True, help="Rime user data directory")
    p.add_argument("--remote-url", required=True, help="WebDAV server URL")
    p.add_argument("--username", default="")
    p.add_argument("--password", default="")
    p.add_argument("--password-env", default="QIWO_WEBDAV_PASSWORD")
    p.add_argument("--device-id", default="")
    p.add_argument("--frontend", default="ibus-rime")
    p.add_argument("--json", action="store_true")
    args = p.parse_args()

    password = args.password
    if args.password_env and not password:
        password = os.environ.get(args.password_env, "")

    device_id = args.device_id or os.uname().nodename
    client = WebDavClient(args.remote_url, args.username, password)
    stats = sync(args.rime_user_dir, client, device_id, args.frontend)

    if args.json:
        print(json.dumps(stats))
    else:
        print(f"Sync: uploaded={stats['uploaded']} downloaded={stats['downloaded']} "
              f"skipped={stats['skipped']} conflicts={stats['conflicts']}")

    total_errors = 0  # errors are printed to stderr
    return 0

if __name__ == "__main__":
    sys.exit(main())
