import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[1] / "scripts/audit-runtime.py"
SPEC = importlib.util.spec_from_file_location("audit_runtime", MODULE_PATH)
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


class RuntimeDependencyTests(unittest.TestCase):
    def test_accepts_declared_shared_sdl_runtime(self):
        AUDIT.validate_dependencies(
            ["libc.so.6", "ld-linux-aarch64.so.1", "libSDL2-2.0.so.0", "libdl.so.2", "libm.so.6"],
            ["SDL2 video backend modules required by the target image"],
        )

    def test_rejects_missing_sdl_runtime(self):
        with self.assertRaisesRegex(RuntimeError, "SDL2 runtime dependency is missing"):
            AUDIT.validate_dependencies(["libc.so.6", "ld-linux-aarch64.so.1"], ["backend"])

    def test_rejects_undeclared_direct_runtime_dependency(self):
        with self.assertRaisesRegex(RuntimeError, "undeclared shared runtime dependency"):
            AUDIT.validate_dependencies(
                ["libc.so.6", "ld-linux-aarch64.so.1", "libSDL2-2.0.so.0", "libasound.so.2"],
                ["backend"],
            )

    def test_rejects_android_and_opengl_core_dependencies(self):
        for name in ("libOpenSLES.so", "libGLESv2.so", "libandroid.so"):
            with self.subTest(name=name), self.assertRaisesRegex(RuntimeError, "forbidden platform dependency"):
                AUDIT.validate_dependencies(
                    ["libc.so.6", "ld-linux-aarch64.so.1", "libSDL2-2.0.so.0", name],
                    ["backend"],
                )

    def test_requires_declared_backend_runtime_requirements(self):
        with self.assertRaisesRegex(RuntimeError, "backend runtime requirements were declared"):
            AUDIT.validate_dependencies(
                ["libc.so.6", "ld-linux-aarch64.so.1", "libSDL2-2.0.so.0"], [],
            )


if __name__ == "__main__":
    unittest.main()
