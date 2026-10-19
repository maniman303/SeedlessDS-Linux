#!/usr/bin/env bash
set -Eeuo pipefail

fail() { printf 'prepare-core: ERROR: %s\n' "$*" >&2; exit 1; }
usage() { printf 'Usage: bash scripts/prepare-core.sh (linux-arm64-native|linux-arm64-cross)\n'; }

[[ $# == 1 ]] || { usage >&2; fail 'select exactly one build preset'; }
preset="$1"
case "$preset" in
    linux-arm64-native|linux-arm64-cross) ;;
    *) usage >&2; fail "unsupported preset: $preset" ;;
esac

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
core="$repo_root/external/SeedlessDS-Core"
manifest="$repo_root/patches/seedlessds-core/manifest.json"
[[ -d "$core/.git" || -f "$core/.git" ]] || fail "core checkout is missing: $core"
[[ -f "$manifest" ]] || fail "core compatibility manifest is missing: $manifest"

python3 - "$core" "$manifest" "$preset" "$repo_root" <<'PY'
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

core, manifest_path, preset, repo = map(Path, sys.argv[1:])
repo = repo.resolve()
core = core.resolve()
manifest_path = manifest_path.resolve()
manifest = json.loads(manifest_path.read_text())
expected_commit = manifest['upstream']['commit']

def run(args, cwd=None, check=True):
    return subprocess.run(args, cwd=cwd, check=check, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)

actual_commit = run(['git', '-C', str(core), 'rev-parse', 'HEAD']).stdout.strip()
if actual_commit != expected_commit:
    raise SystemExit(f"prepare-core: ERROR: core revision {actual_commit} differs from manifest {expected_commit}; review the core update")
status = run(['git', '-C', str(core), 'status', '--porcelain=v1']).stdout.strip()
if status:
    raise SystemExit('prepare-core: ERROR: core checkout contains local changes; review them before staging:\n' + status)
submodules = manifest['upstream'].get('submodules', {})
for relative, pin in submodules.items():
    source = core / relative
    pinned = pin['commit']
    source_commit = None
    if source.exists() and (source / '.git').exists():
        result = run(['git', '-C', str(source), 'rev-parse', 'HEAD'], check=False)
        if result.returncode == 0:
            source_commit = result.stdout.strip()
    if source_commit and source_commit != pinned:
        raise SystemExit(f'prepare-core: ERROR: {relative} is at {source_commit}, expected pinned commit {pinned}')
    if source_commit == pinned:
        continue
    cache = (repo / 'build' / 'downloads' / ('submodule-' + relative.replace('/', '-'))).resolve()
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache_commit = None
    if cache.exists():
        result = run(['git', '-C', str(cache), 'rev-parse', 'HEAD'], check=False)
        if result.returncode == 0:
            cache_commit = result.stdout.strip()
    if cache_commit != pinned:
        raise SystemExit(
            f'prepare-core: ERROR: required local submodule/cache is unavailable or not pinned: '
            f'{relative}; initialize it manually or provide {cache} at {pinned}'
        )
    if run(['git', '-C', str(cache), 'rev-parse', 'HEAD']).stdout.strip() != pinned:
        raise SystemExit(f'prepare-core: ERROR: cached {relative} does not match pinned commit {pinned}')

expected = set(manifest['expected_sources'])
actual = {p.relative_to(core).as_posix() for p in (core / 'src').rglob('*')
          if p.suffix in {'.c', '.S'} and p.is_file()}
if actual != expected:
    added = sorted(actual - expected)
    removed = sorted(expected - actual)
    raise SystemExit(f'prepare-core: ERROR: source inventory drift; added={added}, removed={removed}')

build_root = (repo / 'build' / preset).resolve()
if repo / 'build' not in build_root.parents:
    raise SystemExit('prepare-core: ERROR: unsafe build destination')
build_root.mkdir(parents=True, exist_ok=True)
destination = build_root / 'core-src'
if destination.is_symlink():
    raise SystemExit(f'prepare-core: ERROR: refusing symlink staging path: {destination}')

with tempfile.TemporaryDirectory(prefix='.core-src-stage-', dir=build_root) as temporary:
    staged = Path(temporary) / 'core-src'
    shutil.copytree(core, staged, symlinks=True,
                    ignore=shutil.ignore_patterns('.git'))
    for relative, pin in submodules.items():
        source = core / relative
        if (source / '.git').exists():
            continue
        staged_submodule = staged / relative
        if staged_submodule.exists():
            shutil.rmtree(staged_submodule)
        cache = repo / 'build' / 'downloads' / ('submodule-' + relative.replace('/', '-'))
        shutil.copytree(cache, staged_submodule,
                        ignore=shutil.ignore_patterns('.git'))
    for entry in manifest.get('patches', []):
        patch = repo / 'patches' / 'seedlessds-core' / entry['file']
        digest = hashlib.sha256(patch.read_bytes()).hexdigest()
        if digest != entry['sha256']:
            raise SystemExit(f"prepare-core: ERROR: patch hash mismatch for {entry['file']}")
        run(['patch', '-p1', '--dry-run', '--forward', '--input', str(patch)], cwd=staged)
        run(['patch', '-p1', '--forward', '--input', str(patch)], cwd=staged)
    if destination.exists():
        shutil.rmtree(destination)
    os.replace(staged, destination)

print(f'Prepared core {actual_commit} for {preset} at {destination}')
print(f'Validated {len(expected)} C/assembly sources and {len(manifest.get("patches", []))} ordered patches.')
PY
