# SPDX-License-Identifier: GPL-3.0-or-later
"""Consumer defaults and preflight must never fall through into packaging."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("menu_build", Path(__file__).parents[1] / "tools/build.py")
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class ConsumerBuildOptions(unittest.TestCase):
    def fixture(self, root):
        (root / "product.json").write_text(json.dumps({
            "engine_product": "wii-menu", "version": "0.1.2", "provider": "src/menu_grid.c"}))

    def test_preflight_uses_consumer_defaults_without_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve(); self.fixture(root)
            with patch.object(build, "ROOT", root), patch.object(sys, "argv",
                    ["build.py", "--inputs", str(root / "bundle.zip"), "--check"]), patch.object(build.subprocess, "call", return_value=0) as run:
                self.assertEqual(build.main(), 0)
            command = run.call_args.args[0]
            self.assertEqual(command[command.index("--product") + 1], "wii-menu")
            self.assertEqual(command[command.index("--version") + 1], "0.1.2")
            self.assertIn("--check", command)
            self.assertEqual(command[command.index("--output-dir") + 1], str(root / "dist"))
            self.assertTrue(Path(command[command.index("--build-dir") + 1]).is_relative_to(root / "build"))
            self.assertFalse((root / "build").exists())
            self.assertFalse((root / "dist").exists())
            run.assert_called_once()

    def test_engine_failure_does_not_read_metadata_or_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve(); self.fixture(root)
            with patch.object(build, "ROOT", root), patch.object(sys, "argv",
                    ["build.py", "--inputs-dir", str(root / "bundle")]), patch.object(build.subprocess, "call", return_value=3) as run:
                self.assertEqual(build.main(), 3)
            run.assert_called_once()
            self.assertFalse((root / "build").exists())


if __name__ == "__main__":
    unittest.main()
