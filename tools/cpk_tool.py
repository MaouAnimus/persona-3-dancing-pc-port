#!/usr/bin/env python3
"""
tools/cpk_tool.py — CRI CPK Archive Inspector & Extractor for Persona 3 Dancing
Supports @UTF table decoding and CRILAYLA decompression via hacktools.
"""

import os
import sys
import struct
import argparse

try:
    from hacktools import cmp_cri
    HAS_CRILAYLA = True
except ImportError:
    HAS_CRILAYLA = False


def parse_utf(data):
    if len(data) < 32 or data[:4] != b"@UTF":
        return None
    tbl_size, rows_offset, strings_offset, data_offset, tbl_name, num_cols, row_width, num_rows = struct.unpack(
        ">IIIIIHHI", data[4:32]
    )

    strings_base = 8 + strings_offset
    data_base = 8 + data_offset
    rows_base = 8 + rows_offset

    cols = []
    p = 32
    for _ in range(num_cols):
        flags = data[p]
        p += 1
        name_off = struct.unpack(">I", data[p:p + 4])[0]
        p += 4

        name_start = strings_base + name_off
        name_end = data.find(b"\x00", name_start)
        col_name = data[name_start:name_end].decode("ascii", errors="ignore")
        cols.append({"name": col_name, "flags": flags})

    rows = []
    for r in range(num_rows):
        row_p = rows_base + r * row_width
        row = {}
        for c in cols:
            flags = c["flags"]
            storage = flags & 0xF0
            type_id = flags & 0x0F

            val = None
            if storage in (0x30, 0x50):
                if type_id in (0, 1):
                    val = data[row_p]
                    row_p += 1
                elif type_id in (2, 3):
                    val = struct.unpack(">H", data[row_p:row_p + 2])[0]
                    row_p += 2
                elif type_id in (4, 5):
                    val = struct.unpack(">I", data[row_p:row_p + 4])[0]
                    row_p += 4
                elif type_id in (6, 7):
                    val = struct.unpack(">Q", data[row_p:row_p + 8])[0]
                    row_p += 8
                elif type_id == 8:
                    val = struct.unpack(">f", data[row_p:row_p + 4])[0]
                    row_p += 4
                elif type_id == 10:
                    str_idx = struct.unpack(">I", data[row_p:row_p + 4])[0]
                    row_p += 4
                    s_start = strings_base + str_idx
                    s_end = data.find(b"\x00", s_start)
                    val = data[s_start:s_end].decode("utf-8", errors="ignore")
                elif type_id == 11:
                    data_idx = struct.unpack(">I", data[row_p:row_p + 4])[0]
                    row_p += 4
                    data_len = struct.unpack(">I", data[row_p:row_p + 4])[0]
                    row_p += 4
                    val = data[data_base + data_idx : data_base + data_idx + data_len]
            row[c["name"]] = val
        rows.append(row)

    return rows


def get_cpk_toc(cpk_path):
    with open(cpk_path, "rb") as f:
        hdr = f.read(2048)
        if hdr[:4] != b"CPK ":
            raise ValueError(f"{cpk_path} is not a valid CRI CPK archive.")

        header_rows = parse_utf(hdr[16:])
        if not header_rows:
            raise ValueError("Failed to parse CPK header table.")

        cpk_meta = header_rows[0]
        toc_offset = cpk_meta.get("TocOffset")
        if not toc_offset:
            raise ValueError("No TOC found in CPK.")

        f.seek(toc_offset)
        toc_header = f.read(16)
        if toc_header[:4] != b"TOC ":
            raise ValueError("Invalid TOC header.")

        f.seek(toc_offset + 16)
        toc_utf_header = f.read(32)
        tbl_size = struct.unpack(">I", toc_utf_header[4:8])[0]
        f.seek(toc_offset + 16)
        toc_data = f.read(tbl_size + 8)

        file_entries = parse_utf(toc_data)
        return toc_offset, file_entries


def list_files(cpk_path, filter_str=None):
    toc_offset, entries = get_cpk_toc(cpk_path)
    print(f"Total files in {os.path.basename(cpk_path)}: {len(entries)}")
    count = 0
    for e in entries:
        dir_name = e.get("DirName", "") or ""
        file_name = e.get("FileName", "") or ""
        full_path = (dir_name + "/" + file_name) if dir_name else file_name
        if filter_str and filter_str.lower() not in full_path.lower():
            continue
        file_sz = e.get("FileSize") or 0
        ext_sz = e.get("ExtractSize") or file_sz
        is_cmp = " [CRILAYLA]" if ext_sz > file_sz else ""
        print(f"  {full_path} ({ext_sz} bytes){is_cmp}")
        count += 1
        if count >= 100 and not filter_str:
            print(f"  ... and {len(entries) - 100} more files. Use --filter to search.")
            break


def extract_file_entry(f, toc_offset, entry, out_dir):
    dir_name = entry.get("DirName", "") or ""
    file_name = entry.get("FileName", "") or ""
    rel_path = os.path.join(dir_name, file_name)
    out_file = os.path.join(out_dir, rel_path)

    file_offset = entry.get("FileOffset")
    file_size = entry.get("FileSize")
    extract_size = entry.get("ExtractSize") or file_size

    abs_offset = toc_offset + file_offset
    f.seek(abs_offset)
    raw_data = f.read(file_size)

    if extract_size > file_size and raw_data[:8] == b"CRILAYLA":
        if HAS_CRILAYLA:
            final_data = cmp_cri.decompressCRILAYLA(raw_data)
        else:
            print(f"WARNING: Cannot decompress CRILAYLA for {file_name} (hacktools not installed)")
            final_data = raw_data
    else:
        final_data = raw_data

    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    with open(out_file, "wb") as out_f:
        out_f.write(final_data)
    return out_file


def extract_files(cpk_path, out_dir, filter_str=None, limit=None):
    toc_offset, entries = get_cpk_toc(cpk_path)
    os.makedirs(out_dir, exist_ok=True)

    matched = []
    for e in entries:
        dir_name = e.get("DirName", "") or ""
        file_name = e.get("FileName", "") or ""
        full_path = (dir_name + "/" + file_name) if dir_name else file_name
        if filter_str and filter_str.lower() not in full_path.lower():
            continue
        matched.append(e)

    print(f"Found {len(matched)} matching files. Extracting to: {out_dir}")
    count = 0
    with open(cpk_path, "rb") as f:
        for entry in matched:
            out_path = extract_file_entry(f, toc_offset, entry, out_dir)
            count += 1
            if count <= 15 or count % 50 == 0:
                print(f"[{count}/{len(matched)}] Extracted: {out_path}")
            if limit and count >= limit:
                print(f"Reached limit of {limit} files.")
                break
    print(f"Extraction completed. {count} files extracted.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CRI CPK Inspector & Extractor for Persona 3/5 Dancing")
    parser.add_argument("cpk", help="Path to .cpk file")
    parser.add_argument("--list", action="store_true", help="List files in CPK")
    parser.add_argument("--filter", type=str, default=None, help="Filter files by substring")
    parser.add_argument("--extract", type=str, default=None, help="Extract matching files to directory")
    parser.add_argument("--limit", type=int, default=None, help="Limit number of files to extract")

    args = parser.parse_args()
    if args.list:
        list_files(args.cpk, args.filter)
    elif args.extract:
        extract_files(args.cpk, args.extract, args.filter, args.limit)
    else:
        list_files(args.cpk, args.filter)
