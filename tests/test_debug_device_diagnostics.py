import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "debug-device.py"
SPEC = importlib.util.spec_from_file_location("debug_device", SCRIPT)
debug_device = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(debug_device)


class DiagnosticTailTests(unittest.TestCase):
    def test_run_accepts_frontend_config_and_save_overrides_without_data_dir(self):
        parser = debug_device.root_parser()
        args = parser.parse_args([
            "run", "--binary", "runner", "--inventory", "inventory.json",
            "--rom", "/rom/game.nds", "--frontend-config", "/cfg/custom.json",
            "--save-dir", "", "--frames", "2",
        ])
        self.assertEqual(args.frontend_config, "/cfg/custom.json")
        self.assertEqual(args.save_dir, "")
        self.assertEqual(args.frames, 2)

    def test_run_can_select_rom_from_configured_directory(self):
        parser = debug_device.root_parser()
        args = parser.parse_args([
            "run", "--binary", "runner", "--inventory", "inventory.json",
        ])
        self.assertIsNone(args.rom)
        command = debug_device.rom_discovery_command("/storage/roms with spaces/nds")
        self.assertIn("/storage/roms with spaces/nds", command)
        self.assertIn("[nN][dD][sS]", command)

    def test_config_accepts_private_rom_directory(self):
        values = {
            "ip_address": "192.0.2.10",
            "root_password": "private-test-password",
            "install_dir": "/storage/.config/seedlessds/bin",
            "rom_dir": "/storage/roms/nds",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "device.json"
            path.write_text(json.dumps(values), encoding="utf-8")
            os.chmod(path, 0o600)
            config = debug_device.load_config(path)
        self.assertEqual(config["rom_dir"], "/storage/roms/nds")

    def test_tail_redacts_private_values_and_target_paths(self):
        redactor = debug_device.Redactor(
            ["private-password", "/storage/roms/nds/game.nds", "/storage/.config/seedlessds"]
        )
        raw = (
            "seedlessds[fatal]: signal=SIGBUS address=0x12\n"
            "rom=/storage/roms/nds/game.nds data=/storage/.config/seedlessds "
            "password=private-password\n"
        )

        result = debug_device.sanitized_diagnostic_tail(raw, redactor)

        self.assertIn("signal=SIGBUS", result)
        self.assertNotIn("private-password", result)
        self.assertNotIn("/storage/roms/nds/game.nds", result)
        self.assertNotIn("/storage/.config/seedlessds", result)

    def test_tail_is_bounded_and_terminal_safe(self):
        raw = "\n".join(f"line-{i}" for i in range(40)) + "\n\x1b[31mred\x1b[0m\x01"

        result = debug_device.diagnostic_tail(raw)

        self.assertLessEqual(len(result), 4096)
        self.assertLessEqual(len(result.splitlines()), 32)
        self.assertIn("red?", result)
        self.assertNotIn("line-0", result)
        self.assertIn("line-39", result)


if __name__ == "__main__":
    unittest.main()
