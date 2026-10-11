import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("release_tooling", ROOT / "scripts/release.py")
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def manifest(self):
        return {
            "schema_version": 1, "version": "2.5.2", "tag": "v2.5.2",
            "source_sha": "a" * 40, "idf_version": "6.1.0", "board_count": 1,
            "firmware_count": 1,
            "variants": [{"board": "test", "name": "test", "full_name": "test", "target": "esp32c3"}],
        }

    def image(self, version="2.5.2", chip=5, idf="v6.1", project="xiaozhi"):
        data = bytearray(b"\xff" * 0x11000)
        data[0x8000:0x8004] = b"\xaa\x50\x00\x10"
        struct.pack_into("<II", data, 0x8004, 0x10000, 0x1000)
        data[0x10000] = 0xE9
        struct.pack_into("<H", data, 0x1000C, chip)
        desc = 0x10020
        data[desc:desc + 256] = bytes(256)
        struct.pack_into("<I", data, desc, 0xABCD5432)
        for offset, value in ((16, version), (48, project), (112, idf)):
            encoded = value.encode()
            data[desc + offset:desc + offset + len(encoded)] = encoded
        return data

    def package(self, folder, image=None, member="merged-binary.bin"):
        path = Path(folder) / "v2.5.2_test.zip"
        with zipfile.ZipFile(path, "w") as archive:
            archive.writestr(member, image if image is not None else self.image())
        return path

    def test_full_matrix_keeps_sdk_gated_variants_and_excludes_ci_options(self):
        manifest = release.create_manifest("2.5.2", "a" * 40, (6, 1, 0))
        variants = manifest["variants"]
        self.assertEqual(manifest["firmware_count"], len(variants))
        self.assertEqual(manifest["board_count"], len({v["board"] for v in variants}))
        self.assertIn("esp32s31", {v["target"] for v in variants})
        self.assertTrue(all(not v["full_name"].endswith("-ci") for v in variants))
        self.assertEqual(len({v["full_name"] for v in variants}), len(variants))

    def test_rejects_invalid_version_sha_and_sdk(self):
        for version, sha, sdk in (("2.5.2-rc1", "a" * 40, (6, 1, 0)),
                                  ("2.5.2", "main", (6, 1, 0)),
                                  ("2.5.2", "a" * 40, (6, 0, 2))):
            with self.subTest(version=version, sha=sha, sdk=sdk), self.assertRaises(ValueError):
                release.create_manifest(version, sha, sdk)

    def test_verified_package_has_a_reproducible_checksum(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.package(folder)
            expected = hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n"
            self.assertEqual(release.verify_assets(self.manifest(), Path(folder)), expected)

    def test_missing_extra_and_duplicate_variants_cannot_pass(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(ValueError, "missing"):
                release.verify_assets(self.manifest(), Path(folder))
            self.package(folder)
            extra = Path(folder) / "old.zip"
            extra.touch()
            with self.assertRaisesRegex(ValueError, "unexpected"):
                release.verify_assets(self.manifest(), Path(folder))
            extra.unlink()
            manifest = self.manifest()
            manifest["variants"] *= 2
            manifest["firmware_count"] = 2
            with self.assertRaisesRegex(ValueError, "count"):
                release.verify_assets(manifest, Path(folder))

    def test_wrong_version_chip_sdk_project_and_zip_layout_fail(self):
        for image, member in ((self.image(version="2.5.0"), "merged-binary.bin"),
                              (self.image(chip=9), "merged-binary.bin"),
                              (self.image(idf="v5.5"), "merged-binary.bin"),
                              (self.image(project="another"), "merged-binary.bin"),
                              (self.image(), "xiaozhi.bin"),
                              (bytes(10), "merged-binary.bin")):
            with self.subTest(member=member), tempfile.TemporaryDirectory() as folder:
                self.package(folder, image, member)
                with self.assertRaises(ValueError):
                    release.verify_assets(self.manifest(), Path(folder))

    def test_missing_and_truncated_app_partitions_fail(self):
        missing = self.image()
        missing[0x8000:0x8002] = bytes(2)
        truncated = self.image()
        struct.pack_into("<I", truncated, 0x8004, len(truncated))
        for image in (missing, truncated):
            with self.assertRaises(ValueError):
                release.firmware_metadata(image)

    def test_draft_lookup_uses_the_list_endpoint_and_handles_pagination(self):
        draft = {"id": 123, "tag_name": "v2.5.2", "draft": True}
        first_page = [{"tag_name": f"v0.0.{n}"} for n in range(20)]
        with patch.object(release, "gh_json", side_effect=[first_page, [draft]]) as api:
            self.assertEqual(release.find_release("78/xiaozhi-esp32", "v2.5.2"), draft)
            self.assertEqual([call.args[1] for call in api.call_args_list],
                             ["releases?per_page=20&page=1", "releases?per_page=20&page=2"])
        with patch.object(release, "gh_json", return_value=[]):
            self.assertIsNone(release.find_release("78/xiaozhi-esp32", "v2.5.2"))

    def test_published_release_and_moved_tag_are_never_overwritten(self):
        manifest = self.manifest()
        with patch.object(release, "find_release", return_value={"draft": False}):
            with self.assertRaisesRegex(ValueError, "already published"):
                release.preflight("78/xiaozhi-esp32", manifest)
        with patch.object(release, "find_release", return_value=None), \
                patch.object(release, "gh_json", return_value={"object": {"type": "commit", "sha": "b" * 40}}):
            with self.assertRaisesRegex(ValueError, "different commit"):
                release.preflight("78/xiaozhi-esp32", manifest)

    def test_annotated_tag_and_existing_draft_must_target_the_same_source(self):
        manifest = self.manifest()
        draft = {"draft": True, "target_commitish": manifest["source_sha"]}
        replies = [{"object": {"type": "tag", "sha": "b" * 40}},
                   {"object": {"type": "commit", "sha": manifest["source_sha"]}}]
        with patch.object(release, "find_release", return_value=draft), \
                patch.object(release, "gh_json", side_effect=replies):
            self.assertEqual(release.preflight("78/xiaozhi-esp32", manifest), draft)
        with patch.object(release, "find_release", return_value={"draft": True, "target_commitish": "main"}):
            with self.assertRaisesRegex(ValueError, "different commit"):
                release.preflight("78/xiaozhi-esp32", manifest)

    def test_publication_waits_for_complete_remote_assets_and_matching_digests(self):
        for corrupt in (False, True):
            with self.subTest(corrupt=corrupt), tempfile.TemporaryDirectory() as folder:
                assets = Path(folder) / "assets"
                assets.mkdir()
                self.package(assets)
                meta = Path(folder) / "meta"
                draft = {"id": 123, "draft": True, "target_commitish": self.manifest()["source_sha"]}
                def remote(repository, endpoint, **kwargs):
                    if endpoint == "releases/latest":
                        return None
                    self.assertEqual(endpoint, "releases/123")
                    files = list(assets.iterdir()) + [meta / "release-manifest.json", meta / "SHA256SUMS"]
                    attachments = [{"name": p.name, "size": p.stat().st_size,
                                    "digest": "sha256:" + hashlib.sha256(p.read_bytes()).hexdigest()}
                                   for p in files]
                    if corrupt:
                        attachments[0]["digest"] = "sha256:" + "0" * 64
                    return {"html_url": "https://example.com/release", "assets": attachments}
                with patch.object(release, "preflight", return_value=None), \
                        patch.object(release, "find_release", return_value=draft), \
                        patch.object(release, "gh_json", side_effect=remote), \
                        patch.object(release.subprocess, "check_output", return_value=json.dumps({"body": "Changes"})), \
                        patch.object(release.subprocess, "run") as run:
                    if corrupt:
                        with self.assertRaisesRegex(ValueError, "digest mismatch"):
                            release.publish("78/xiaozhi-esp32", self.manifest(), assets, meta,
                                            "https://example.com/run", True)
                        self.assertTrue(all("--draft=false" not in c.args[0] for c in run.call_args_list))
                    else:
                        release.publish("78/xiaozhi-esp32", self.manifest(), assets, meta,
                                        "https://example.com/run", True)
                        self.assertIn("--draft=false", run.call_args_list[-1].args[0])

    def test_upload_failure_leaves_the_release_unpublished(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = Path(folder) / "assets"
            assets.mkdir()
            self.package(assets)
            def command(args, **kwargs):
                if args[1:3] == ["release", "upload"]:
                    raise RuntimeError("upload failed")
            with patch.object(release, "preflight", return_value=None), \
                    patch.object(release, "gh_json", return_value=None), \
                    patch.object(release.subprocess, "check_output", return_value=json.dumps({"body": "Changes"})), \
                    patch.object(release.subprocess, "run", side_effect=command) as run:
                with self.assertRaisesRegex(RuntimeError, "upload failed"):
                    release.publish("78/xiaozhi-esp32", self.manifest(), assets,
                                    Path(folder) / "meta", "https://example.com/run", True)
                self.assertTrue(all("--draft=false" not in c.args[0] for c in run.call_args_list))


if __name__ == "__main__":
    unittest.main()
