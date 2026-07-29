#!/usr/bin/env python3
"""Small source/JAR compatibility patches used by the standalone builder."""

from __future__ import annotations

import struct
import zipfile
from pathlib import Path


def patch_romized_rms_null_placeholder(rom_path: Path) -> bool:
    text = rom_path.read_text(encoding="latin-1")
    marker = "Standalone RMS compatibility: Gameloft creates placeholder"
    if marker in text:
        return False
    old = '''        { /* nds/pstros/rms/RmsRecord: <init>(int, byte[], int, int) */
            0x2a, 0xd7, 0x00, 0x01, 0x2a, 0x1b, 0xce, 0x00, 0x01, 0x2a, 
            0x15, 0x04, 0xbc, 0x08, 0xce, 0x00, 0x00, 0x2c, 0x1d, 0x2a, 
            0xcc, 0x00, 0x00, 0x03, 0x15, 0x04, 0xd8, 0x00, 0x02, 0xb1
        },'''
    new = '''        { /* nds/pstros/rms/RmsRecord: <init>(int, byte[], int, int) */
            /* Standalone RMS compatibility: Gameloft creates placeholder
             * records with data == null and length == 0. */
            0x2a, 0xd7, 0x00, 0x01, 0x2a, 0x1b, 0xce, 0x00, 0x01, 0x2a,
            0x2c, 0xce, 0x00, 0x00, 0xb1, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },'''
    if old not in text:
        raise RuntimeError("Cannot locate ROMized RmsRecord constructor")
    rom_path.write_text(text.replace(old, new, 1), encoding="latin-1", newline="\n")
    return True


def patch_romized_rms_autosave(rom_path: Path) -> bool:
    text = rom_path.read_text(encoding="latin-1")
    if "Standalone RMS autosave: persist after every mutation" in text:
        return False
    old_pool = """            ROM_CPOOL_METHOD(nds_pstros_rms_RmsManager, 0) /* nds.pstros.rms.RmsManager getInstance() */,
            ROM_CPOOL_CLASS(javax_microedition_rms_RecordStoreFullException)
"""
    new_pool = """            ROM_CPOOL_METHOD(nds_pstros_rms_RmsManager, 0) /* nds.pstros.rms.RmsManager getInstance() */,
            ROM_CPOOL_METHOD(nds_pstros_rms_RmsManager, 6) /* void saveData() */
"""
    if old_pool not in text:
        raise RuntimeError("Cannot locate RecordStore spare constant-pool slot")
    old_method = """        { /* javax/microedition/rms/RecordStore: void setEvent(int, int) */
            0x2a, 0xcc, 0x00, 0x02, 0xc7, 0x00, 0x04, 0xb1, 0x2a, 0xcc, 
            0x00, 0x02, 0xb6, 0x00, 0x3b, 0x3e, 0x03, 0x36, 0x05, 0x15, 
            0x05, 0x1d, 0xa2, 0x00, 0x55, 0x2a, 0xcc, 0x00, 0x02, 0x15, 
            0x05, 0xb6, 0x00, 0x3c, 0xdd, 0x00, 0x3d, 0x3a, 0x04, 0x1b, 
            0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00, 
            0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1c, 
            0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x34, 0x19, 0x04, 
            0x2a, 0x1c, 0xb9, 0x00, 0x3e, 0x03, 0x00, 0xa7, 0x00, 0x18, 
            0x19, 0x04, 0x2a, 0x1c, 0xb9, 0x00, 0x3f, 0x03, 0x00, 0xa7, 
            0x00, 0x0c, 0x19, 0x04, 0x2a, 0x1c, 0xb9, 0x00, 0x40, 0x03, 
            0x00, 0x84, 0x05, 0x01, 0xa7, 0xff, 0xab, 0xb1
        },
"""
    new_bytes = [0xD1, 0x00, 0x0D, 0xB6, 0x00, 0x44, 0xB1] + [0x00] * 101
    lines = []
    for i in range(0, len(new_bytes), 10):
        chunk = new_bytes[i:i + 10]
        lines.append("            " + ", ".join(f"0x{x:02x}" for x in chunk) + ("," if i + 10 < len(new_bytes) else ""))
    new_method = """        { /* javax/microedition/rms/RecordStore: void setEvent(int, int) */
            /* Standalone RMS autosave: persist after every mutation. */
""" + "\n".join(lines) + "\n        },\n"
    if old_method not in text:
        raise RuntimeError("Cannot locate ROMized RecordStore.setEvent()")
    text = text.replace(old_pool, new_pool, 1).replace(old_method, new_method, 1)
    rom_path.write_text(text, encoding="latin-1", newline="\n")
    return True


def _u2(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from(">H", data, offset)[0], offset + 2


def _u4(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from(">I", data, offset)[0], offset + 4


def patch_gameloft_rms_defaults(class_data: bytes) -> tuple[bytes, bool]:
    data = bytearray(class_data)
    if len(data) < 16 or data[:4] != b"\xca\xfe\xba\xbe":
        return class_data, False
    offset = 8
    cp_count, offset = _u2(data, offset)
    cp_utf8: dict[int, str] = {}
    index = 1
    while index < cp_count:
        tag = data[offset]; offset += 1
        if tag == 1:
            length, offset = _u2(data, offset)
            cp_utf8[index] = bytes(data[offset:offset + length]).decode("utf-8", errors="replace")
            offset += length
        elif tag in (3, 4): offset += 4
        elif tag in (5, 6): offset += 8; index += 1
        elif tag in (7, 8): offset += 2
        elif tag in (9, 10, 11, 12): offset += 4
        else: return class_data, False
        index += 1
    offset += 6
    count, offset = _u2(data, offset); offset += count * 2
    count, offset = _u2(data, offset)
    for _ in range(count):
        offset += 6
        attrs, offset = _u2(data, offset)
        for _ in range(attrs):
            offset += 2
            length, offset = _u4(data, offset); offset += length
    methods, offset = _u2(data, offset)
    target = "(Ljavax/microedition/rms/RecordStore;ILjava/lang/Object;)Ljava/lang/Object;"
    for _ in range(methods):
        offset += 2
        name_index, offset = _u2(data, offset)
        desc_index, offset = _u2(data, offset)
        attrs, offset = _u2(data, offset)
        is_target = cp_utf8.get(name_index) == "RMS_load" and cp_utf8.get(desc_index) == target
        for _ in range(attrs):
            attr_name, offset = _u2(data, offset)
            attr_len, offset = _u4(data, offset)
            attr_start = offset
            if is_target and cp_utf8.get(attr_name) == "Code":
                pos = attr_start + 4
                code_len, pos = _u4(data, pos)
                code_start = pos
                pos = code_start + code_len
                exc_count, pos = _u2(data, pos)
                handlers = []
                for _ in range(exc_count):
                    _, pos = _u2(data, pos); _, pos = _u2(data, pos)
                    handler, pos = _u2(data, pos); _, pos = _u2(data, pos)
                    handlers.append(handler)
                if not handlers or len(set(handlers)) != 1:
                    return class_data, False
                handler = code_start + handlers[0]
                if data[handler:handler + 4] == b"\x57\xa7\x00\x0c":
                    return bytes(data), False
                original = bytes(data[handler:handler + 13])
                if len(original) != 13 or original[0] != 0x4B or original[1] != 0xBB or original[-1] != 0xBF:
                    return class_data, False
                data[handler:handler + 13] = b"\x57\xa7\x00\x0c" + b"\x00" * 9
                return bytes(data), True
            offset = attr_start + attr_len
    return class_data, False


def patch_embedded_jar_compatibility(jar_path: Path) -> bool:
    with zipfile.ZipFile(jar_path, "r") as src:
        infos = src.infolist()
        try:
            original = src.read("Reg/RMS.class")
        except KeyError:
            return False
        patched, changed = patch_gameloft_rms_defaults(original)
        if not changed:
            return False
        entries = [(info, patched if info.filename == "Reg/RMS.class" else src.read(info)) for info in infos]
    temp = jar_path.with_suffix(jar_path.suffix + ".tmp")
    try:
        with zipfile.ZipFile(temp, "w") as dst:
            for info, payload in entries:
                dst.writestr(info, payload, compress_type=info.compress_type)
        temp.replace(jar_path)
    finally:
        try: temp.unlink()
        except OSError: pass
    return True
