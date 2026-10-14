#!/usr/bin/env python3
"""Lossless, offline decoder for the Virus TI updater's FORM bank containers.

Never writes input files and never produces a hardware-flashable modification.
The bank mapping follows REA's LoadTIOSChunk decompilation; the DSP word-block
layout is independently documented by dsp56300/gearmulator's ROMFile::readChunks.
"""
import argparse
import collections
import hashlib
import html
import json
import math
from pathlib import Path
import re
import struct

BANK_SIZE = 0x8000
SOURCES = {
    "updater_decompilation": "REA evidence ev_e295405d59f852ef9658c3069d89a8ff25631a4938a3c76caa7caa3452efd10a: LoadTIOSChunk at 0x1da48",
    "record_format": "https://github.com/dsp56300/gearmulator/blob/897b31893b3a7c1a98aaa3909e57c785cb6ade82/source/axel/virusLib/unpacker.cpp",
    "dsp_format": "https://github.com/dsp56300/gearmulator/blob/897b31893b3a7c1a98aaa3909e57c785cb6ade82/source/axel/virusLib/romfile.cpp",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def extract_images(bundle, destination):
    data = bundle.read_bytes()
    require(data[:4] == b"FORM" and int.from_bytes(data[4:8], "big") == len(data) - 8,
            "outer firmware FORM length mismatch")
    chunks = []
    offset = 8
    while offset < len(data):
        require(offset + 8 <= len(data), "truncated outer chunk header")
        size = int.from_bytes(data[offset + 4:offset + 8], "big")
        require(offset + 8 + size <= len(data), "truncated outer payload")
        chunks.append((data[offset:offset + 4], offset + 8, size))
        offset += 8 + size
    require(chunks and chunks[0][0] == b"TABL", "missing outer name table")
    _, start, length = chunks[0]
    table = data[start:start + length]
    require(table and table[-1] == 0, "unterminated name table")
    names = table[1:-1].decode("ascii").split("\0")
    require(table[0] == len(names) == len(chunks) - 1, "name count mismatch")
    require(len(set(names)) == len(names), "duplicate outer names")
    destination.mkdir(parents=True, exist_ok=True)
    extracted = []
    for name, (_, start, length) in zip(names, chunks[1:]):
        if name not in ("vti.bin", "vti_2.bin", "vti_snow.bin"):
            continue
        content = data[start:start + length]
        output = destination / name
        if output.exists():
            require(output.read_bytes() == content, f"refusing to overwrite different image: {output}")
        else:
            output.write_bytes(content)
        extracted.append({"name": name, "outer_offset": start, "size": length, "sha256": sha(content)})
    require(len(extracted) == 3, "expected three firmware images")
    write_json(destination / "extraction-manifest.json", {
        "source": str(bundle.resolve()), "source_sha256": sha(data), "images": extracted,
        "validation": "Complete bounded FORM/TABL parse; existing images matched byte for byte"})


def strings(data):
    return [{"offset": m.start(), "text": m.group().decode("ascii")}
            for m in re.finditer(rb"[ -~]{5,}", data)]


def entropy(data):
    counts = collections.Counter(data)
    return round(-sum((v / len(data)) * math.log2(v / len(data))
                      for v in counts.values()), 5)


def smooth_windows(data):
    """Rank 256-sample windows; smoothness is not proof of waveform semantics."""
    out = []
    for endian in (">", "<"):
        for off in range(0, len(data) - 511, 512):
            vals = struct.unpack_from(endian + "256h", data, off)
            span = max(vals) - min(vals)
            if span < 8192 or len(set(vals)) < 128:
                continue
            delta = sum(abs(b - a) for a, b in zip(vals, vals[1:])) / 255
            score = delta / span
            if score < 0.04:
                out.append({"offset": off, "sample_format": "s16" + ("be" if endian == ">" else "le"),
                            "samples": 256, "range": span, "mean_absolute_delta_over_range": round(score, 6),
                            "caution": "Heuristic only: may be a lookup table, not an oscillator waveform."})
    return sorted(out, key=lambda v: v["mean_absolute_delta_over_range"])[:16]


def decode_payload(payload, tag):
    if len(payload) == BANK_SIZE:
        return payload, b"", b"", {"format": "raw"}
    require(len(payload) in (0x8c00, 0x8c02), f"{tag}: unsupported payload length {len(payload)}")
    headers = bytearray()
    raw = bytearray()
    for i in range(1024):
        header = payload[35 * i:35 * i + 3]
        headers.extend(header)
        raw.extend(payload[35 * i + 3:35 * i + 35])
        if len(payload) == 0x8c02:
            require(int.from_bytes(header[1:], "big") == i * 32, f"{tag}: wrong byte offset in record {i}")
        else:
            require(header[1] == i % 256, f"{tag}: wrong record counter in record {i}")
    require(len(set(headers[::3])) == 1, f"{tag}: changing embedded block id")
    if len(payload) == 0x8c00:
        require(len(set(headers[2::3])) == 1, f"{tag}: changing third header byte")
    tail = payload[35 * 1024:]
    rebuilt = b"".join(headers[i * 3:i * 3 + 3] + raw[i * 32:i * 32 + 32]
                       for i in range(1024)) + tail
    require(rebuilt == payload, f"{tag}: lossless record reconstruction failed")
    return bytes(raw), bytes(headers), tail, {
        "format": "1024_records_of_3_header_bytes_plus_32_data_bytes",
        "record_address_validation": "all 1024 big-endian byte offsets valid" if tail else "all 1024 wrapping 8-bit counters valid",
        "embedded_block_id": headers[0],
        "third_header_byte": headers[2] if not tail else None,
        "trailer_hex": tail.hex(),
        "trailer_semantics": "not established by this decoder" if tail else "absent",
        "payload_roundtrip_identical": True,
    }


def inspect(path, out_root, names_xml=None):
    original = path.read_bytes()
    require(original[:4] == b"FORM", f"{path}: no FORM signature")
    require(int.from_bytes(original[4:8], "big") == len(original) - 8, f"{path}: FORM size mismatch")
    out = out_root / path.stem
    out.mkdir(parents=True, exist_ok=True)
    entries, bank_ids, tags, decoded = [], set(), set(), {}
    reconstructed = bytearray(original[:8])
    offset = 8
    while offset < len(original):
        require(offset + 8 <= len(original), "truncated chunk header")
        chunk_header = original[offset:offset + 8]
        tag = chunk_header[:4].decode("ascii")
        require(re.fullmatch(r"[FPS][0-9]{3}", tag) is not None, f"unsupported tag {tag}")
        size = int.from_bytes(chunk_header[4:], "big")
        require(offset + 8 + size <= len(original), f"{tag}: truncated payload")
        require(tag not in tags, f"duplicate tag {tag}")
        bank = int(tag[1:]) + (128 if tag[0] in "FP" else 0)
        require(bank not in bank_ids, f"duplicate updater bank {bank}")
        tags.add(tag)
        bank_ids.add(bank)
        payload = original[offset + 8:offset + 8 + size]
        raw, headers, tail, metadata = decode_payload(payload, tag)
        decoded[tag] = raw
        (out / f"{tag}.bin").write_bytes(raw)
        if headers:
            (out / f"{tag}.record-headers.bin").write_bytes(headers)
            (out / f"{tag}.trailer.bin").write_bytes(tail)
        # Re-read the generated files to verify that deliverables suffice to
        # reconstruct the original encoded bytes, including uninterpreted bytes.
        disk_raw = (out / f"{tag}.bin").read_bytes()
        if headers:
            disk_headers = (out / f"{tag}.record-headers.bin").read_bytes()
            disk_tail = (out / f"{tag}.trailer.bin").read_bytes()
            rebuilt = b"".join(disk_headers[i * 3:i * 3 + 3] + disk_raw[i * 32:i * 32 + 32]
                               for i in range(1024)) + disk_tail
        else:
            rebuilt = disk_raw
        reconstructed.extend(chunk_header + rebuilt)
        entry = {"tag": tag, "source_chunk_offset": offset, "source_payload_offset": offset + 8,
                 "encoded_size": size, "decoded_size": len(raw), "updater_bank": bank,
                 "updater_buffer_byte_offset": bank * BANK_SIZE,
                 "decoded_sha256": sha(raw), "encoded_sha256": sha(payload),
                 "entropy_bits_per_byte": entropy(raw), "zero_bytes": raw.count(0),
                 "ff_bytes": raw.count(255), "encoding": metadata}
        entries.append(entry)
        offset += 8 + size
    require(offset == len(original), "unconsumed input bytes")
    require(reconstructed == original, "complete FORM reconstruction differs")
    require(sha(path.read_bytes()) == sha(original), "input changed during inspection")

    all_strings = {tag: strings(raw) for tag, raw in decoded.items()}
    write_json(out / "strings.json", all_strings)
    name_mapping = None
    if names_xml is not None:
        xml_bytes = names_xml.read_bytes()
        # This narrowly reads the known plain Entry list, without an XML parser
        # or resolving any external XML entities/resources.
        value_list = re.search(r'<Valuelist\b[^>]*name="Val Name Wavetables"[^>]*>(.*?)</Valuelist>',
                               xml_bytes.decode("utf-8"), re.S)
        require(value_list is not None, "wavetable value list missing from names XML")
        names = [(int(i), html.unescape(n)) for i, n in re.findall(r'<Entry index="(\d+)">([^<]*)</Entry>', value_list.group(1))]
        require(len({i for i, _ in names}) == len(names), "duplicate wavetable indices in XML")
        hits = []
        for table_index, name in names:
            encoded_name = name.encode("ascii")
            locations = []
            for entry in entries:
                if not entry["tag"].startswith("F"):
                    continue
                raw = decoded[entry["tag"]]
                start = 0
                while (hit := raw.find(encoded_name, start)) >= 0:
                    encoded_offset = hit if entry["encoded_size"] == BANK_SIZE else (hit // 32) * 35 + 3 + hit % 32
                    locations.append({"tag": entry["tag"], "decoded_bank_offset": hit,
                                      "updater_buffer_offset": entry["updater_buffer_byte_offset"] + hit,
                                      "original_image_start_offset": entry["source_payload_offset"] + encoded_offset})
                    start = hit + 1
            hits.append({"index": table_index, "name": name, "name_byte_matches": locations})
        name_mapping = {"source": str(names_xml.resolve()), "source_sha256": sha(xml_bytes),
                        "entries": hits, "name_count": len(names),
                        "names_found_in_F_banks": sum(bool(h["name_byte_matches"]) for h in hits),
                        "caution": "Exact name-byte matches identify labels only, not waveform sample storage; short names may match substrings."}
        write_json(out / "wavetable-name-offsets.json", name_mapping)
    wavish = {tag: candidates for tag, raw in decoded.items()
              if tag.startswith("F") and (candidates := smooth_windows(raw))}
    write_json(out / "smooth-data-candidates.json", wavish)
    families = {}
    for family in "FPS":
        selected = [e for e in entries if e["tag"].startswith(family)]
        if not selected:
            continue
        data = b"".join(decoded[e["tag"]] for e in selected)
        (out / f"{family}.concat.bin").write_bytes(data)
        families[family] = {"size": len(data), "sha256": sha(data), "order": [e["tag"] for e in selected],
                            "caution": "Container order concatenation, not a physical address image; gaps are omitted."}
    dsp = []
    command_bytes = bytearray()
    for index in range(15):
        tag = f"F{20 + index:03}"
        require(tag in decoded, f"missing primary DSP word block {tag}")
        raw = decoded[tag]
        require(raw[0] == 14 - index, f"{tag}: unexpected DSP sequence id")
        words = ((raw[1] - 1) << 8) | raw[2]
        require(words >= 0 and 3 + words * 3 <= len(raw), f"{tag}: DSP word count outside bank")
        body = raw[3:3 + words * 3]
        (out / f"{tag}.dsp24be.bin").write_bytes(body)
        dsp.append({"tag": tag, "sequence_id": raw[0], "word_count": words, "data_offset_in_bank": 3,
                    "data_end_in_bank": 3 + words * 3, "sha256": sha(body)})
        command_bytes.extend(body)
    boot_words = int.from_bytes(command_bytes[:3], "big")
    boot_address = int.from_bytes(command_bytes[3:6], "big")
    require((boot_words + 2) * 3 <= len(command_bytes), "DSP boot image truncated")
    boot = command_bytes[6:(boot_words + 2) * 3]
    stream = command_bytes[(boot_words + 2) * 3:]
    (out / "dsp-boot24be.bin").write_bytes(boot)
    (out / "dsp-command-stream24be.bin").write_bytes(stream)
    manifest = {"input": str(path.resolve()), "input_size": len(original), "input_sha256": sha(original),
                "sources": SOURCES, "validation": {"form_bounds": True, "unique_chunk_tags": True,
                "unique_updater_banks": True, "all_record_addresses_or_counters": True,
                "lossless_reconstruction_from_output_files": True, "input_unchanged": True},
                "address_caution": "updater_buffer_byte_offset is the host loader's logical bank mapping; it is not a verified hardware flash address",
                "chunks": entries, "families": families, "primary_dsp_word_blocks": dsp,
                "dsp_boot": {"word_count": boot_words, "load_word_address": boot_address, "sha256": sha(boot)},
                "dsp_command_stream": {"word_count": len(stream) // 3, "sha256": sha(stream)},
                "wavetable_names": {"source": name_mapping["source"], "name_count": name_mapping["name_count"],
                                    "names_found_in_F_banks": name_mapping["names_found_in_F_banks"],
                                    "offset_map": "wavetable-name-offsets.json"} if name_mapping else None,
                "analysis_caution": "Bank classification and DSP structure do not identify oscillator waveforms. No signatures or device acceptance have been verified."}
    write_json(out / "manifest.json", manifest)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path)
    parser.add_argument("--output", type=Path, default=Path("analysis/firmware-images/decoded"))
    parser.add_argument("--names-xml", type=Path, help="Optional Virus Control remotedevice.xml to map wavetable labels")
    parser.add_argument("--bundle", type=Path, help="Extract all three images from outer firmware.bin before decoding")
    args = parser.parse_args()
    if args.bundle:
        require(not args.inputs, "--bundle uses its extracted images; do not supply other inputs")
        extract_images(args.bundle, args.output.parent)
        args.inputs = [args.output.parent / name for name in ("vti.bin", "vti_2.bin", "vti_snow.bin")]
    inputs = args.inputs or [Path("analysis/firmware-images") / name for name in ("vti.bin", "vti_2.bin", "vti_snow.bin")]
    manifests = [inspect(path, args.output, args.names_xml) for path in inputs]
    comparison = []
    for other in manifests[1:]:
        main_chunks = {c["tag"]: c for c in manifests[0]["chunks"]}
        other_chunks = {c["tag"]: c for c in other["chunks"]}
        common = sorted(main_chunks.keys() & other_chunks.keys())
        comparison.append({"base": manifests[0]["input"], "other": other["input"],
                           "identical_decoded_banks": [t for t in common if main_chunks[t]["decoded_sha256"] == other_chunks[t]["decoded_sha256"]],
                           "differing_decoded_banks": [t for t in common if main_chunks[t]["decoded_sha256"] != other_chunks[t]["decoded_sha256"]]})
    write_json(args.output / "comparison.json", comparison)
    print(json.dumps({"images": [{"path": m["input"], "banks": len(m["chunks"]),
                                  "sha256": m["input_sha256"], "validation": m["validation"],
                                  "families": {k: v["size"] for k, v in m["families"].items()},
                                  "dsp_boot": m["dsp_boot"], "dsp_command_stream": m["dsp_command_stream"]}
                                 for m in manifests]}, indent=2))


if __name__ == "__main__":
    main()
