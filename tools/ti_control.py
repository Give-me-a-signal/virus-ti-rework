#!/usr/bin/env python3
"""Control a loaded Virus TI Rework plug-in, without its editor or a bench host."""
import argparse
import json
import os
from pathlib import Path
import socket
import stat
import sys
import time

COMMANDS = ("status", "connect", "disconnect", "test-tone", "banks", "presets", "load", "load-file", "state-save", "state-load", "cutoff", "parts", "emulator", "note-on", "note-off", "route")


def request(path, command):
    if (command.get("command") if isinstance(command, dict) else command) not in COMMANDS:
        raise ValueError("Unsupported command")
    path = Path(path)
    parent = path.parent.lstat()
    endpoint = path.lstat()
    if (not stat.S_ISDIR(parent.st_mode) or parent.st_uid != os.getuid()
            or stat.S_IMODE(parent.st_mode) != 0o700
            or not stat.S_ISSOCK(endpoint.st_mode) or endpoint.st_uid != os.getuid()
            or stat.S_IMODE(endpoint.st_mode) != 0o600):
        raise ValueError("Control endpoint must be a private socket owned by this user")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(135 if isinstance(command, dict) and command.get("command", "").startswith("state-") else 2)
        connection.connect(str(path))
        wire = json.dumps(command, ensure_ascii=True) if isinstance(command, dict) else command
        if len(wire) >= 4096:
            raise ValueError("Command exceeds 4095 bytes")
        connection.sendall((wire + "\n").encode("ascii"))
        data = bytearray()
        while not data.endswith(b"\n"):
            part = connection.recv(4096)
            if not part:
                raise ConnectionError("Control endpoint closed without a complete response")
            data.extend(part)
            if len(data) > 65536:
                raise ValueError("Oversized control response")
    result = json.loads(data)
    if result.get("protocol") != 1:
        raise ValueError("Unsupported control protocol")
    return result


def discover():
    found = []
    for path in sorted(Path(f"/tmp/virus-ti-control-{os.getuid()}").glob("*.sock")):
        try:
            state = request(path, "status")
            found.append({"socket": str(path), **state})
        except (OSError, ValueError):
            pass  # A crashed/scanning host may leave an unresponsive socket.
    return found


def restorations():
    """Read completed/in-progress recovery reports after the DAW has exited."""
    root = Path.home() / "Library/Application Support/VirusTIRework"
    results = []
    for path in root.glob("*/restoration-status.json"):
        try:
            data = json.loads(path.read_text())
            results.append({"snapshot_directory": str(path.parent), **data})
        except (OSError, ValueError):
            continue
    return sorted(results, key=lambda value: value.get("time", 0), reverse=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("list", "restorations") + COMMANDS)
    parser.add_argument("--socket", help="Exact instance socket from 'list'")
    parser.add_argument("--wait", action="store_true", help="Wait for command completion")
    parser.add_argument("--bank", type=int, help="Bank ID returned by banks")
    parser.add_argument("--preset", type=int, default=0, help="Zero-based preset index")
    parser.add_argument("--path", help="Local preset or project-state file")
    parser.add_argument("--value", type=int, help="Cutoff value 0–127")
    parser.add_argument("--part", type=int, default=1, help="Target TI part 1–16")
    parser.add_argument("--rom", help="Local outer firmware bundle for OsTIrus only")
    parser.add_argument("--note", type=int, default=60)
    parser.add_argument("--velocity", type=int, default=40)
    parser.add_argument("--bus", type=int, default=1, help="USB stereo return 1–3")
    args = parser.parse_args()
    if args.command == "restorations":
        print(json.dumps(restorations(), indent=2))
        return 0
    if args.command == "list":
        print(json.dumps(discover(), indent=2))
        return 0
    path = args.socket
    if not path:
        instances = [x for x in discover() if x.get("host_processing") or x.get("connected")]
        if len(instances) != 1:
            raise ValueError(f"Found {len(instances)} active instances; use list and --socket explicitly")
        path = instances[0]["socket"]
    command = args.command
    if command in ("presets", "load"):
        if args.bank is None:
            raise ValueError("--bank is required")
        command = {"command": command, "bank": args.bank, "preset": args.preset}
    elif command in ("load-file", "state-save", "state-load"):
        if not args.path:
            raise ValueError("--path is required")
        command = {"command": command, "path": str(Path(args.path).resolve()), "preset": args.preset}
    if command == "cutoff":
        command = {"command": command, "value": args.value}
    if command == "emulator":
        if not args.rom:
            raise ValueError("--rom is required for the emulator")
        command = {"command": command, "rom": str(Path(args.rom).resolve())}
    elif command in ("note-on", "note-off"):
        command = {"command": command, "note": args.note, "velocity": args.velocity}
    elif command == "route":
        command = {"command": command, "bus": args.bus}
    elif command == "test-tone":
        command = {"command": command}
    if isinstance(command, dict):
        command["part"] = args.part
    result = request(path, command)
    if args.wait and result.get("request_id"):
        request_id = result["request_id"]
        deadline = time.monotonic() + 180
        while result.get("completed_request_id", 0) < request_id:
            if time.monotonic() >= deadline:
                raise TimeoutError("Command still pending; inspect status before retrying")
            time.sleep(0.1)
            result = request(path, "status")
        # Completion means the worker finished, not that hardware succeeded.
        text = result.get("status", "")
        success = result.get("last_command_succeeded", (text.startswith("Connected") if args.command == "connect" else
                   "Stock test tone" in text if args.command == "test-tone" else
                   text.startswith("Disconnected") and "needs attention" not in text))
        result["command_succeeded"] = success
    if args.wait and args.command == "state-load" and result.get("ok"):
        deadline = time.monotonic() + 180
        while result.get("busy") or result.get("state_recall_pending"):
            if time.monotonic() >= deadline:
                raise TimeoutError("Project recall still pending; inspect status")
            time.sleep(0.1)
            result = request(path, "status")
        result["command_succeeded"] = result.get("last_command_succeeded", False)
    print(json.dumps({"socket": path, **result}, indent=2))
    return 0 if result.get("ok") and result.get("command_succeeded", True) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, TimeoutError) as error:
        print(json.dumps({"ok": False, "error": str(error)}), file=sys.stderr)
        sys.exit(1)
