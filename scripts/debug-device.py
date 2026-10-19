#!/usr/bin/env python3
"""Deploy and run the headless ARM64 runner on a configured Linux console."""
from __future__ import annotations

import argparse
import getpass
import hashlib
import ipaddress
import json
import os
import pwd
import re
import shlex
import shutil
import socket
import stat
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG_KEYS = {"ip_address", "root_password", "install_dir"}
PLACEHOLDERS = {"<CONSOLE_IP>", "<ROOT_SSH_PASSWORD>", "<ABSOLUTE_BINARY_INSTALL_DIRECTORY>"}
EVIDENCE = None

class Failure(Exception):
    def __init__(self, code: int, stage: str, message: str):
        super().__init__(message)
        self.code, self.stage, self.message = code, stage, message

class Redactor:
    def __init__(self, values):
        self.values = sorted({v for v in values if isinstance(v, str) and v}, key=len, reverse=True)

    def clean(self, value):
        text = str(value)
        for secret in self.values:
            forms = {secret, json.dumps(secret)[1:-1], shlex.quote(secret)}
            for form in forms:
                if form:
                    text = text.replace(form, "[REDACTED]")
        return text


def emit(stage, status, message):
    global EVIDENCE
    print(f"stage={stage} status={status} {message}", file=sys.stderr if status != "passed" else sys.stdout)
    if EVIDENCE is not None:
        EVIDENCE["stages"][stage] = status
        EVIDENCE["observations"].append(message)
        evidence_dir = ROOT / "build" / "validation" / "device"
        evidence_dir.mkdir(parents=True, exist_ok=True)
        target = evidence_dir / (EVIDENCE["session"] + ".json")
        temporary = target.with_suffix(".tmp")
        temporary.write_text(json.dumps(EVIDENCE, indent=2) + "\n", encoding="utf-8")
        os.chmod(temporary, 0o600)
        temporary.replace(target)


def diagnostic_tail(output):
    """Return a short, terminal-safe tail for local evidence and failure output."""
    output = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", output)
    output = "".join(char if char in "\n\t" or ord(char) >= 32 else "?" for char in output)
    lines = output.splitlines()[-32:]
    return "\n".join(lines)[-4096:]


def sanitized_diagnostic_tail(output, redactor):
    return diagnostic_tail(redactor.clean(output))


def default_config():
    return ROOT / ".device.local"


def load_config(path):
    try:
        info = path.lstat()
    except OSError:
        raise Failure(2, "configuration", "configuration file is missing or unreadable")
    if not stat.S_ISREG(info.st_mode) or path.is_symlink():
        raise Failure(2, "configuration", "configuration must be a regular, nonsymlink file")
    if hasattr(os, "getuid") and info.st_uid != os.getuid():
        raise Failure(2, "configuration", "configuration must be owned by the current user")
    if info.st_mode & 0o077:
        raise Failure(2, "configuration", "configuration permissions must exclude group and other users")

    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("duplicate key")
            result[key] = value
        return result
    try:
        data = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs)
    except Exception:
        raise Failure(2, "configuration", "configuration contains invalid JSON")
    if not isinstance(data, dict):
        raise Failure(2, "configuration", "configuration must be a JSON object")
    if set(data) != CONFIG_KEYS:
        raise Failure(2, "configuration", "configuration must contain exactly ip_address, root_password, install_dir")
    if not all(isinstance(data[k], str) for k in CONFIG_KEYS):
        raise Failure(2, "configuration", "configuration fields must be strings")
    try:
        ipaddress.ip_address(data["ip_address"])
    except ValueError:
        raise Failure(2, "configuration", "invalid field ip_address; provide a literal IPv4 or IPv6 address")
    password = data["root_password"]
    if not password or password in PLACEHOLDERS or any(ord(c) < 32 or ord(c) == 127 for c in password):
        raise Failure(2, "configuration", "invalid field root_password")
    dest = data["install_dir"]
    p = Path(dest)
    if dest in PLACEHOLDERS or not p.is_absolute() or os.path.normpath(dest) == "/" or any(x == ".." for x in p.parts) or any(ord(c) < 32 or ord(c) == 127 for c in dest):
        raise Failure(2, "configuration", "invalid field install_dir; use an absolute POSIX directory without traversal")
    return data


def root_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("check", "run"):
        p = sub.add_parser(name)
        p.add_argument("--config", type=Path)
        if name == "run":
            p.add_argument("--binary", type=Path, required=True)
            p.add_argument("--inventory", type=Path, required=True)
            p.add_argument("--rom", required=True)
            p.add_argument("--data-dir", required=True)
            p.add_argument("--frames", type=int, default=120)
            p.add_argument("--timeout", type=int, default=30)
    return parser


def check_tools():
    missing = [x for x in ("ssh", "sshpass", "readelf") if not shutil.which(x)]
    if missing:
        raise Failure(2, "host_tools", "missing host tools: " + ", ".join(missing))


def remote_arg(config):
    ip = ipaddress.ip_address(config["ip_address"])
    return f"root@{ip}"


def ssh_command(config, remote, *, input_data=None, timeout=10):
    password_r, password_w = os.pipe()
    try:
        os.write(password_w, config["root_password"].encode() + b"\n")
        os.close(password_w)
        password_w = -1
        argv = ["sshpass", "-d", str(password_r), "ssh", "-F", "/dev/null", "-p", "22",
                "-o", "BatchMode=no", "-o", "PreferredAuthentications=password,keyboard-interactive",
                "-o", "PubkeyAuthentication=no", "-o", "NumberOfPasswordPrompts=1",
                "-o", "StrictHostKeyChecking=yes", "-o", "ForwardAgent=no", "-o", "ClearAllForwardings=yes",
                "-o", "ControlMaster=no", "-o", "ControlPath=none", "-o", "ConnectTimeout=10",
                "--", remote_arg(config), remote]
        return subprocess.run(argv, input=input_data, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=timeout, pass_fds=(password_r,), check=False)
    except subprocess.TimeoutExpired:
        raise Failure(3, "connection", "SSH operation timed out")
    finally:
        try: os.close(password_r)
        except OSError: pass
        if password_w >= 0:
            try: os.close(password_w)
            except OSError: pass


def classify_ssh(result, stage):
    if result.returncode == 0:
        return
    output = (result.stdout + result.stderr).decode("utf-8", "replace").lower()
    if stage == "preflight" and result.returncode != 255:
        raise Failure(4, "compatibility", "target architecture, runtime utility, ROM or writable data directory check failed")
    if "host key verification failed" in output or "remote host identification has changed" in output:
        msg = "host verification failed; verify the console fingerprint and enroll it interactively"
    elif "permission denied" in output:
        msg = "SSH authentication failed; confirm root password authentication is available"
    elif result.returncode == 255:
        msg = "SSH connection failed; check reachability and verified host key"
    else:
        msg = f"remote {stage} command failed with transport status {result.returncode}"
    raise Failure(3 if stage in ("connection", "preflight") else 5, stage, msg)


def run_remote(config, command, stage, timeout=10, payload=None):
    result = ssh_command(config, command, input_data=payload, timeout=timeout)
    classify_ssh(result, stage)
    return result.stdout.decode("utf-8", "replace")


def release_lock_command(lock, token):
    qlock, qtoken = shlex.quote(lock), shlex.quote(token)
    marker = qlock + "/session"
    return ("if [ \"$(cat -- " + shlex.quote(marker) + " 2>/dev/null || true)\" = " + qtoken +
            " ]; then rm -f -- " + shlex.quote(marker) + " && rmdir -- " + qlock + "; fi")


def audit(binary, inventory_path):
    try:
        requested_binary = Path(binary)
        if requested_binary.is_symlink():
            raise ValueError()
        binary = requested_binary.resolve(strict=True)
        if not binary.is_file() or not os.access(binary, os.R_OK):
            raise ValueError()
        data = json.loads(inventory_path.read_text(encoding="utf-8"))
        header = subprocess.run([shutil.which("readelf"), "-h", str(binary)], capture_output=True, text=True, check=True).stdout
        dynamic = subprocess.run([shutil.which("readelf"), "-d", str(binary)], capture_output=True, text=True, check=True).stdout
        versions = subprocess.run([shutil.which("readelf"), "--version-info", str(binary)], capture_output=True, text=True, check=True).stdout
        if "AArch64" not in header or data.get("elf_machine", "").find("AArch64") < 0:
            raise ValueError()
        if Path(data.get("artifact", "")).resolve() != binary:
            raise ValueError()
        if data.get("size_bytes", binary.stat().st_size) != binary.stat().st_size:
            raise ValueError()
        digest = hashlib.sha256(binary.read_bytes()).hexdigest()
        expected = data.get("sha256")
        if not expected or expected != digest or data.get("size_bytes") != binary.stat().st_size:
            raise ValueError()
        needed = sorted(set(re.findall(r"Shared library: \[([^]]+)\]", dynamic)))
        if data.get("shared_dependencies", needed) != needed:
            raise ValueError()
        glibc = sorted(set(re.findall(r"Name: (GLIBC_[0-9.]+)", versions)))
        max_glibc = max(glibc, key=lambda v: tuple(map(int, v.split("_")[1].split(".")))) if glibc else None
        if data.get("maximum_glibc_symbol_version") != max_glibc:
            raise ValueError()
        interpreter = re.search(r"Requesting program interpreter:\s*([^]]+)", subprocess.run(
            [shutil.which("readelf"), "-l", str(binary)], capture_output=True, text=True, check=True).stdout)
        actual_interpreter = interpreter.group(1) if interpreter else None
        if data.get("interpreter") != actual_interpreter:
            raise ValueError()
        return {"path": binary, "sha256": digest, "size": binary.stat().st_size,
                "dependencies": needed, "glibc": max_glibc,
                "interpreter": actual_interpreter, "inventory": data}
    except Failure:
        raise
    except Exception:
        raise Failure(2, "artifact", "binary or runtime inventory is missing, stale, or not a valid AArch64 ELF")


def remote_preflight(config, artifact, rom, data_dir):
    script = r'''set -eu
[ "$(uname -s)" = Linux ]
[ "$(uname -m)" = aarch64 ]
for c in sh mkdir cat chmod mv rm sha256sum timeout ldd; do command -v "$c" >/dev/null; done
timeout --signal=TERM --kill-after=5s 1 sh -c ':'
[ -r "$1" ]
[ -d "$2" ] && [ -w "$2" ]
if [ -n "$3" ]; then [ -x "$3" ]; fi
printf 'device=%s kernel=%s arch=%s\n' "$(. /etc/os-release; printf '%s' "${PRETTY_NAME:-Linux}")" "$(uname -r)" "$(uname -m)"
LC_ALL=C ldd --version 2>&1 | head -1
'''
    interpreter = artifact.get("interpreter") or ""
    cmd = "sh -c " + shlex.quote(script) + " sh " + shlex.quote(rom) + " " + shlex.quote(data_dir) + " " + shlex.quote(interpreter)
    result = ssh_command(config, cmd, timeout=15)
    if result.returncode:
        classify_ssh(result, "preflight")
        raise Failure(4, "compatibility", "target architecture, runtime utility, ROM or writable data directory check failed")
    out = result.stdout.decode("utf-8", "replace")
    if artifact["glibc"]:
        match = re.search(r"ldd.*?([0-9]+\.[0-9]+)", out, re.I)
        req = tuple(map(int, artifact["glibc"].split("_")[1].split(".")))
        if not match:
            raise Failure(4, "compatibility", "could not determine target glibc version")
        if tuple(map(int, match.group(1).split("."))) < req:
            raise Failure(4, "compatibility", "target glibc is older than the artifact requirement")
    return out


def main():
    args = root_parser().parse_args()
    config_path = (args.config.expanduser().absolute() if args.config else default_config())
    config = load_config(config_path)
    redactor = Redactor([config[k] for k in CONFIG_KEYS])
    global EVIDENCE
    revision = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True)
    if args.command == "run":
        EVIDENCE = {"session": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8],
                    "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                    "revision": revision.stdout.strip() if revision.returncode == 0 else "unknown",
                    "artifact_sha256": None, "preset": None, "device": {}, "stages": {},
                    "runner_exit": None, "transport_exit": None, "result": None, "observations": []}
    check_tools()
    if args.command == "check":
        emit("configuration", "passed", "configuration and host tools are valid")
        return 0
    if not (1 <= args.frames <= 2**64 - 1) or args.timeout <= 0:
        raise Failure(2, "arguments", "frames must be 1..UINT64_MAX and timeout must be positive")
    rom = args.rom
    data_dir = args.data_dir
    if not Path(rom).is_absolute() or not Path(data_dir).is_absolute():
        raise Failure(2, "arguments", "ROM and data directory must be absolute target paths")
    redactor = Redactor([config[k] for k in CONFIG_KEYS] + [rom, data_dir])
    artifact = audit(args.binary, args.inventory)
    EVIDENCE["artifact_sha256"] = artifact["sha256"]
    EVIDENCE["preset"] = artifact["inventory"].get("preset")
    emit("artifact", "passed", f"AArch64 ELF validated sha256={artifact['sha256']}")
    # Explicit interactive enrollment is a prerequisite; automation never accepts new host keys.
    try:
        run_remote(config, "printf connected", "connection")
    except Failure as e:
        raise e
    emit("connection", "passed", "verified root SSH connection established")
    target_info = remote_preflight(config, artifact, rom, data_dir)
    EVIDENCE["device"] = {"runtime": target_info.strip()}
    emit("compatibility", "passed", "target architecture, runtime and user assets are compatible")
    dest = config["install_dir"]
    final = dest.rstrip("/") + "/seedlessds-headless"
    token = uuid.uuid4().hex
    lock = dest.rstrip("/") + "/.seedlessds-debug.lock"
    temp = dest.rstrip("/") + "/.seedlessds-headless." + token + ".tmp"
    lockcmd = ("set -eu; mkdir -p " + shlex.quote(dest) + "; [ -d " + shlex.quote(dest) + " ]; [ ! -L " + shlex.quote(dest) +
               " ]; [ ! -L " + shlex.quote(final) + " ]; mkdir " + shlex.quote(lock) +
               "; if printf '%s\\n' " + shlex.quote(token) + " > " + shlex.quote(lock + "/session") +
               "; then :; else rm -f -- " + shlex.quote(lock + "/session") + "; rmdir -- " + shlex.quote(lock) + "; exit 1; fi")
    try:
        run_remote(config, lockcmd, "deployment")
    except Failure:
        raise Failure(5, "deployment", "destination unavailable or locked; inspect stale lock before recovery")
    try:
        upload = "umask 077; cat > " + shlex.quote(temp) + " && printf '%s\\n' " + shlex.quote(artifact["sha256"] + "  " + temp) + " | sha256sum -c - && chmod 0755 " + shlex.quote(temp) + " && mv -f -- " + shlex.quote(temp) + " " + shlex.quote(final)
        result = ssh_command(config, upload, input_data=artifact["path"].read_bytes(), timeout=120)
        classify_ssh(result, "deployment")
    except Exception as e:
        try: run_remote(config, "rm -f -- " + shlex.quote(temp) + "; " + release_lock_command(lock, token), "deployment")
        except Exception: pass
        if isinstance(e, Failure): raise Failure(5, "deployment", e.message)
        raise Failure(5, "deployment", "transfer or verification failed; prior executable was preserved")
    emit("deployment", "passed", f"verified and promoted sha256={artifact['sha256']}")
    try:
        marker_token = uuid.uuid4().hex
        command = ("set -eu; if " + shlex.quote(final) + " --help >/dev/null 2>&1; then "
                   "if timeout --signal=TERM --kill-after=5s " + str(args.timeout) + " " + shlex.quote(final) +
                   " --data-dir " + shlex.quote(data_dir) + " --frames " + str(args.frames) + " " + shlex.quote(rom) +
                   "; then status=0; else status=$?; fi; else status=125; fi; printf '\\n__SEEDLESS_WRAPPER_" + marker_token + "__:%s\\n' \"$status\"; exit 0")
        result = ssh_command(config, command, timeout=args.timeout + 15)
    except Failure:
        try: run_remote(config, release_lock_command(lock, token), "cleanup")
        except Failure: pass
        raise Failure(7, "execution", "execution completion is unconfirmed after transport loss")
    output = redactor.clean((result.stdout + result.stderr).decode("utf-8", "replace"))
    try:
        run_remote(config, release_lock_command(lock, token), "cleanup")
    except Failure:
        pass
    marker = re.search(r"__SEEDLESS_WRAPPER_" + marker_token + r"__:(\d+)", output)
    output = re.sub(r"^.*__SEEDLESS_WRAPPER_" + marker_token + r"__:\d+.*(?:\n|$)", "", output, flags=re.M)
    record = re.search(rf"result=completed frames={args.frames} requested={args.frames}(?:\r?\n|$)", output)
    if result.returncode == 255 or not marker:
        EVIDENCE["transport_exit"] = result.returncode
        tail = sanitized_diagnostic_tail(output, redactor)
        if tail:
            EVIDENCE["runner_diagnostics"] = tail
            print("sanitized remote diagnostics:", file=sys.stderr)
            print(tail, file=sys.stderr)
        raise Failure(7, "execution", "execution completion is unconfirmed; no valid wrapper status received")
    EVIDENCE["runner_exit"] = int(marker.group(1))
    EVIDENCE["transport_exit"] = result.returncode
    EVIDENCE["result"] = record.group(0).strip() if record else "missing_or_invalid"
    if int(marker.group(1)) or not record:
        tail = sanitized_diagnostic_tail(output, redactor)
        if tail:
            EVIDENCE["runner_diagnostics"] = tail
            print("sanitized remote diagnostics:", file=sys.stderr)
            print(tail, file=sys.stderr)
        raise Failure(6, "execution", f"runner exit={marker.group(1)}; exact bounded completion record is missing")
    emit("execution", "passed", f"runner exit=0 result=completed frames={args.frames} requested={args.frames}")
    print(output, end="" if output.endswith("\n") else "\n")
    return 0


def cli():
    args = root_parser().parse_args()
    config_path = (args.config.expanduser().absolute() if args.config else default_config())
    try:
        return main()
    except Failure as e:
        config = None
        try: config = load_config(config_path)
        except Failure: pass
        redactor = Redactor([config[k] for k in CONFIG_KEYS] if config else [])
        emit(e.stage, "failed", redactor.clean(e.message))
        return e.code
    except KeyboardInterrupt:
        emit("execution", "blocked", "interrupted; completion is unconfirmed")
        return 7
    except Exception:
        emit("internal", "failed", "unexpected internal error; sensitive values suppressed")
        return 6

if __name__ == "__main__":
    raise SystemExit(cli())
