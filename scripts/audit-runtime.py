#!/usr/bin/env python3
"""Audit the target ELF and record its OS runtime requirements."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def command(args):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, check=False)
    if result.returncode:
        raise RuntimeError(f"{' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--preset', required=True)
    parser.add_argument('--readelf', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    readelf = args.readelf
    header = command([readelf, '-h', str(binary)])
    dynamic = command([readelf, '-d', str(binary)])
    versions = command([readelf, '--version-info', str(binary)])
    undefined = command([readelf, '-Ws', str(binary)])
    machine = re.search(r'Machine:\s*(.+)', header)
    interpreter_out = command([readelf, '-l', str(binary)])
    interpreter = re.search(r'Requesting program interpreter:\s*([^]]+)', interpreter_out)
    needed = sorted(set(re.findall(r'Shared library: \[([^]]+)\]', dynamic)))
    version_names = sorted(set(re.findall(r'Name: (GLIBC_[0-9.]+)', versions)),
                           key=lambda x: tuple(int(n) for n in x.split('_')[1].split('.')))
    forbidden = re.compile(r'android|opensl|libegl|libgles|libgl\.so', re.I)
    forbidden_names = sorted(name for name in needed if forbidden.search(name))
    unexpected = sorted(name for name in needed if name not in {'libc.so.6', 'ld-linux-aarch64.so.1'})
    if not machine or 'AArch64' not in machine.group(1):
        raise RuntimeError('ELF machine is not AArch64')
    if forbidden_names:
        raise RuntimeError('forbidden platform dependency: ' + ', '.join(forbidden_names))
    if unexpected:
        raise RuntimeError('undeclared shared runtime dependency: ' + ', '.join(unexpected))
    if re.search(r'UND.*(__android_log_print|slCreateEngine|glViewport|glBindTexture)', undefined):
        raise RuntimeError('unresolved Android, OpenSL or OpenGL symbol remains')
    inventory = {
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'preset': args.preset,
        'artifact': str(binary),
        'size_bytes': binary.stat().st_size,
        'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
        'elf_machine': machine.group(1).strip(),
        'interpreter': interpreter.group(1) if interpreter else None,
        'shared_dependencies': needed,
        'maximum_glibc_symbol_version': version_names[-1] if version_names else None,
        'glibc_symbol_versions': version_names,
        'statically_embedded_application_libraries': [
            {'name': 'SeedlessDS-Core', 'revision': '1cae6f9d8ad7dc651a48452a5ebe23fd8c347d12'},
            {'name': 'Lua', 'revision': '3c55790ebee7148281cd00f44aa293e456b4da6d'},
            {'name': 'libarchive', 'version': '3.8.9'},
            {'name': 'unarr', 'version': '1.1.1'},
            {'name': 'LZMA SDK', 'version': '9.20'},
            {'name': 'zlib', 'version': '1.3.2'},
            {'name': 'libm', 'source': 'target compiler static archive'},
        ],
        'retained_os_runtime': needed,
        'static_linkage_exceptions': [],
        'undefined_symbol_policy': 'ELF is linked with --no-undefined; unresolved symbols listed above are dynamic OS imports only.',
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(inventory, indent=2) + '\n')
    print(f"Runtime inventory: {args.output}")
    print(f"ELF: {inventory['elf_machine']}; shared runtime: {', '.join(needed)}")
    print(f"Artifact SHA-256: {inventory['sha256']} ({inventory['size_bytes']} bytes)")
    print(f"Maximum GLIBC symbol version: {inventory['maximum_glibc_symbol_version']}")
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f'audit-runtime: ERROR: {error}', file=sys.stderr)
        raise SystemExit(1)
