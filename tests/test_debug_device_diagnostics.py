import importlib.util
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "debug-device.py"
SPEC = importlib.util.spec_from_file_location("debug_device", SCRIPT)
debug_device = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(debug_device)


class DiagnosticTailTests(unittest.TestCase):
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
