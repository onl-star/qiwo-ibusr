#!/usr/bin/env python3

import importlib.util
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("qiwo_sync", ROOT / "qiwo_sync.py")
qiwo_sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qiwo_sync)


class FakeWebDavClient(qiwo_sync.WebDavClient):
    def __init__(self, base_url, responses):
        super().__init__(base_url)
        self.responses = list(responses)
        self.calls = []

    def _req_url(self, method, url, data=None, headers=None):
        self.calls.append((method, url, data, headers))
        if not self.responses:
            return 500, b"unexpected request"
        return self.responses.pop(0)


class WebDavUploadTests(unittest.TestCase):
    def test_upload_retries_after_creating_missing_root(self):
        client = FakeWebDavClient(
            "https://dav.example.com/remote.php/dav/files/user/qiwo",
            [
                (404, b""),  # first PUT
                (404, b""),  # PROPFIND root
                (201, b""),  # MKCOL root
                (201, b""),  # second PUT
            ],
        )

        client.upload("default.custom.yaml", b"content")

        self.assertEqual(
            [call[0] for call in client.calls],
            ["PUT", "PROPFIND", "MKCOL", "PUT"],
        )
        self.assertEqual(
            client.calls[0][1],
            "https://dav.example.com/remote.php/dav/files/user/qiwo/default.custom.yaml",
        )

    def test_upload_reports_directory_creation_failure(self):
        client = FakeWebDavClient(
            "https://dav.example.com/remote.php/dav/files/user/qiwo",
            [(404, b"")],
        )

        with self.assertRaisesRegex(Exception, "Unable to create WebDAV directory"):
            client.upload("nested/default.custom.yaml", b"content")


if __name__ == "__main__":
    unittest.main()
