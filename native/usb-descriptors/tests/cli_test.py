"""Offline end-to-end tests only. Never opens a device session."""
import json
from pathlib import Path
import subprocess
import tempfile

binary = Path(__file__).resolve().parents[1] / "build/virus-usb-descriptors"


def invoke(*args, status=0):
    result = subprocess.run([str(binary), *args], capture_output=True, text=True)
    assert result.returncode == status, (result.returncode, result.stdout, result.stderr)
    return result


assert "Usage:" in invoke().stdout
assert "Usage:" in invoke("--help").stdout
invoke("--unknown", status=2)
invoke("--inspect-connected", "extra", status=2)
with tempfile.TemporaryDirectory() as root:
    path = Path(root) / "synthetic.bin"
    # Synthetic one-interface, one-isochronous-endpoint USB2 configuration.
    fixture = bytes([9, 2, 25, 0, 1, 1, 0, 128, 50,
                     9, 4, 0, 0, 1, 1, 2, 0, 0,
                     7, 5, 129, 5, 192, 0, 1])
    path.write_bytes(fixture)
    report = json.loads(invoke("--file", str(path), "--speed", "full").stdout)
    assert report["mode"] == "offline"
    assert report["configuration"]["raw_hex"] == fixture.hex()
    endpoint = report["configuration"]["alternates"][0]["endpoints"][0]
    assert endpoint["bytes_per_service_interval"] == 192
    assert endpoint["service_interval_ns"] == 1000000
    report = json.loads(invoke("--file", str(path)).stdout)
    assert "bytes_per_service_interval" not in report["configuration"]["alternates"][0]["endpoints"][0]
    invoke("--file", str(path), "--speed", "fake", status=2)
    path.write_bytes(fixture[:-1])
    report = json.loads(invoke("--file", str(path), status=4).stdout)
    assert report["configuration"]["valid"] is False
    path.write_bytes(b"")
    invoke("--file", str(path), status=4)
    path.write_bytes(bytes(65536))
    invoke("--file", str(path), status=2)
    invoke("--file", str(path) + ".absent", status=2)
print("Offline CLI tests passed (synthetic fixture; no hardware access).")
