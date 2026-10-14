#!/usr/bin/env python3
"""Offline plug-in/MIDI inventory. Never loads or executes vendor binaries.

Run with /usr/bin/python3 tools/inventory_plugin_midi.py from the project root.
XML attributes remain strings: this does not implement the vendor XML language.
"""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import plistlib
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "analysis/package-expanded"


def identity(path):
    return {"path": str(path.relative_to(ROOT)),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def run(command):
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f"{command!r}: {result.stderr}")
    return {"command": command, "returncode": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr}


def tree(node):
    result = {"tag": node.tag, "attributes": node.attrib}
    if node.text and node.text.strip():
        result["text"] = node.text.strip()
    if len(node):
        result["children"] = [tree(child) for child in node]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xml-only', action='store_true', help='Skip native symbol inspection for local setup')
    args = parser.parse_args()
    binaries = {}
    for path in ([] if args.xml_only else sorted(BASE.rglob("*"))):
        text = str(path)
        if not path.is_file() or path.parent.name != "MacOS":
            continue
        if not any(name in text for name in ("Core_components.pkg", "Driver_package_10.9.pkg")):
            continue
        if any(name in text for name in ("Uninstall", "Control Center", ".dpm", "Extensions")):
            continue
        item = identity(path)
        item["plist"] = plistlib.loads((path.parent.parent / "Info.plist").read_bytes())
        item["symbols"] = run(["nm", "-arch", "x86_64", "-nm", str(path)])
        item["dependencies"] = run(["otool", "-arch", "x86_64", "-L", str(path)])
        # __cstring VM addresses refer specifically to x86_64, not FAT file offsets.
        item["cstring"] = run(["otool", "-arch", "x86_64", "-v", "-s", "__TEXT", "__cstring", str(path)])
        binaries[item["path"]] = item
    (ROOT / "analysis/plugin-midi-symbol-inventory.json").write_text(
        json.dumps(binaries, indent=2, default=str) + "\n")

    resources = BASE / "Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources"
    catalog = {"scope": "Shipped XML declarations; runtime selection and semantics unverified.",
               "files": {}}
    for name in ("remotedevice.xml", "remotedevicePR.xml", "editordevice.xml",
                 "automation.xml", "automationPR.xml", "editor.xml", "editorPR.xml"):
        path = resources / name
        root = ET.parse(path).getroot()
        item = identity(path)
        item["root"] = {"tag": root.tag, "attributes": root.attrib}
        item["tag_counts"] = dict(collections.Counter(node.tag for node in root.iter()))
        if name.startswith("remotedevice") or name == "editordevice.xml":
            item["parameter_sets"] = [tree(node) for node in root.findall("./ParameterSets/ParameterSet")]
            item["midi_messages"] = [tree(node) for node in root.findall("./MIDIMessages/MIDIMessage")]
            item["value_lists"] = [tree(node) for node in root.findall("./Valuelists/Valuelist")]
            item["conditions"] = [tree(node) for node in root.findall("./Conditionsets/*")]
            item["initialisation"] = [tree(node) for node in root.findall("./Initialisation/*")]
        elif name.startswith("automation"):
            rows = [node.attrib for node in root]
            item["automation"] = rows
            item["parts"] = sorted({int(row["Part"]) for row in rows})
            item["duplicate_indices"] = [key for key, count in collections.Counter(row["Index"] for row in rows).items() if count > 1]
        else:
            item["parameter_bindings"] = [dict(node.attrib, widget=node.tag) for node in root.iter() if "Parameter" in node.attrib]
        catalog["files"][name] = item
    (ROOT / "analysis/plugin-parameter-catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
    print(f"Inventoried {len(binaries)} binaries and {len(catalog['files'])} XML files.")
    for name, item in catalog["files"].items():
        counts = item["tag_counts"]
        print(name, "parameters", counts.get("Parameter", 0), "automation", counts.get("Automation", 0), "messages", counts.get("MIDIMessage", 0))


if __name__ == "__main__":
    main()
