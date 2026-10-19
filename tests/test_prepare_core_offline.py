import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PrepareCoreOfflineTests(unittest.TestCase):
    def test_missing_local_submodule_fails_without_remote_git_command(self):
        manifest = json.loads((ROOT / "patches/seedlessds-core/manifest.json").read_text())
        expected = manifest["upstream"]["commit"]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            core = root / "external/SeedlessDS-Core"
            core.mkdir(parents=True)
            (core / ".git").write_text("fixture gitdir\n")
            (root / "patches/seedlessds-core").mkdir(parents=True)
            (root / "patches/seedlessds-core/manifest.json").write_text(json.dumps(manifest))
            (root / "scripts").mkdir()
            script = (ROOT / "scripts/prepare-core.sh").read_text()
            script = script.replace(
                'repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"',
                f'repo_root="{root}"',
            )
            (root / "scripts/prepare-core.sh").write_text(script)

            bin_dir = root / "bin"
            bin_dir.mkdir()
            git_log = root / "git.log"
            fake_git = bin_dir / "git"
            fake_git.write_text(
                "#!/usr/bin/env python3\n"
                "import os, sys\n"
                "with open(os.environ['GIT_LOG'], 'a') as f: f.write(' '.join(sys.argv[1:])+'\\n')\n"
                "args = sys.argv[1:]\n"
                f"if args[-2:] == ['rev-parse', 'HEAD'] and args[1].endswith('SeedlessDS-Core'): print('{expected}'); raise SystemExit(0)\n"
                "if args[-1:] == ['--porcelain=v1']: raise SystemExit(0)\n"
                "if args[-2:] == ['rev-parse', 'HEAD']: raise SystemExit(1)\n"
                "if args and args[0] == 'clone': raise SystemExit('remote git clone must not be attempted')\n"
                "raise SystemExit(2)\n"
            )
            fake_git.chmod(0o755)
            environment = os.environ.copy()
            environment["PATH"] = f"{bin_dir}:{environment['PATH']}"
            environment["GIT_LOG"] = str(git_log)

            result = subprocess.run(
                ["bash", str(root / "scripts/prepare-core.sh"), "linux-arm64-native"],
                text=True,
                capture_output=True,
                env=environment,
                check=False,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("required local submodule/cache is unavailable", result.stderr)
            self.assertNotIn("remote git clone must not be attempted", result.stderr)
            self.assertNotIn("clone", git_log.read_text())


if __name__ == "__main__":
    unittest.main()
