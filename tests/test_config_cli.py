import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(os.environ.get("CONFIG_PROBE"), "run through tests/config/CMakeLists.txt")
class ConfigCliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe = os.environ.get("CONFIG_PROBE", "")

    def test_help_without_rom_or_config(self):
        with tempfile.TemporaryDirectory() as temp:
            env = dict(os.environ, HOME=temp)
            p = subprocess.run([self.probe, "--help"], env=env, capture_output=True, text=True)
            self.assertEqual(p.returncode, 0)
            self.assertIn("--config", p.stdout)
            self.assertFalse((Path(temp) / ".config/seedlessds/config.json").exists())

    def test_missing_rom_exits_before_config_io(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp) / "home"
            home.mkdir()
            env = dict(os.environ, HOME=str(home))
            p = subprocess.run([self.probe], env=env, capture_output=True, text=True)
            self.assertEqual(p.returncode, 2)
            self.assertIn("ROM path is required", p.stderr)
            self.assertFalse((home / ".config/seedlessds/config.json").exists())

    def test_custom_config_creation_is_authoritative_and_defaults_complete(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); home = root / "home"; home.mkdir()
            default = home / ".config/seedlessds/config.json"
            default.parent.mkdir(parents=True); default.write_text("invalid default")
            custom = root / "custom folder/config.json"
            env = dict(os.environ, HOME=str(home))
            p = subprocess.run([self.probe, "--config", str(custom), "--save-dir", "", "game.nds"], env=env, capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stderr)
            cfg = json.loads(custom.read_text())
            self.assertEqual(cfg["firmware"]["bios_arm7"], str(home / ".config/seedlessds/bios/drastic_bios_arm7.bin"))
            self.assertEqual(cfg["user"]["birthday_month"], 3)
            self.assertEqual(cfg["user"]["birthday_day"], 2)
            self.assertEqual(default.read_text(), "invalid default")

    def test_bad_existing_config_is_never_replaced(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); custom = root / "config.json"; custom.write_text('{"duplicate":1,"duplicate":2}')
            original = custom.read_bytes()
            p = subprocess.run([self.probe, "--config", str(custom), "game.nds"], capture_output=True, text=True)
            self.assertEqual(p.returncode, 1)
            self.assertEqual(custom.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
