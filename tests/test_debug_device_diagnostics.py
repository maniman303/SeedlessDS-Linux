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
        self.assertIsNone(args.config)

    def test_run_can_inherit_console_wayland_environment(self):
        args = debug_device.root_parser().parse_args([
            "run", "--binary", "runner", "--inventory", "inventory.json",
            "--console-session",
        ])
        self.assertTrue(args.console_session)
        self.assertEqual(
            debug_device.parse_console_environment(
                "DISPLAY=:0.0\nWAYLAND_DISPLAY=wayland-1\nXDG_RUNTIME_DIR=/run/user/0\n"),
            {"DISPLAY": ":0.0", "WAYLAND_DISPLAY": "wayland-1",
             "XDG_RUNTIME_DIR": "/run/user/0"},
        )
        with self.assertRaises(debug_device.Failure):
            debug_device.parse_console_environment("DISPLAY=:0.0\n")

    def test_frontend_arguments_are_separate_and_shell_quoted(self):
        command = debug_device.frontend_launch_command(
            "/storage/bin/seedlessds", "/cfg/custom file.json",
            "/save dir;safe", 120, "/roms/a game.nds",
        )
        self.assertIn("--config '/cfg/custom file.json'", command)
        self.assertIn("--save-dir '/save dir;safe'", command)
        self.assertIn("--frames 120 '/roms/a game.nds'", command)
        self.assertNotIn("--data-dir", command)
        self.assertNotIn("--frontend-config", command)

    def test_result_parser_accepts_only_exact_requested_completion(self):
        self.assertEqual(
            debug_device.completed_record("result=completed frames=120 requested=120\n", 120).group(0).strip(),
            "result=completed frames=120 requested=120",
        )
        for output in (
            "result=completed frames=119 requested=120\n",
            "result=closed frames=119\n",
            "result=failed frames=0 operation=video\n",
        ):
            self.assertIsNone(debug_device.completed_record(output, 120))

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
