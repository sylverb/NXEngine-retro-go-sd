#!/usr/bin/env python3
"""Extract NXEngine support files from freeware Doukutsu.exe (1.0.0.6 / AGTP).

Mirrors third_party/nxengine/extract/{extractfiles,extractpxt,extractstages}.cpp
so CI can build cavestory.nxpk without launching the SDL host UI.

Usage:
  python3 scripts/extract_doukutsu.py CaveStory/Doukutsu.exe -C CaveStory
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

HEADER_LEN = 25

CREDIT_HEADER = bytes(
    [
        0x42, 0x4D, 0x76, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x76, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0xA0, 0x00,
        0x00, 0x00, 0xF0, 0x00, 0x00,
    ]
)
PIXEL_HEADER = bytes(
    [
        0x42, 0x4D, 0x76, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x76, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0xA0, 0x00,
        0x00, 0x00, 0x10, 0x00, 0x00,
    ]
)

# (relpath, offset, length, crc32, header_or_None)
FILES = [
    ("endpic/credit01.bmp", 0x117047, 19293, 0xEB87B19B, CREDIT_HEADER),
    ("endpic/credit02.bmp", 0x11BBAF, 19293, 0x239C1A37, CREDIT_HEADER),
    ("endpic/credit03.bmp", 0x120717, 19293, 0x4398BBDA, CREDIT_HEADER),
    ("endpic/credit04.bmp", 0x12527F, 19293, 0x44BAE3AC, CREDIT_HEADER),
    ("endpic/credit05.bmp", 0x129DE7, 19293, 0xD1B876AD, CREDIT_HEADER),
    ("endpic/credit06.bmp", 0x12E94F, 19293, 0x5A60082E, CREDIT_HEADER),
    ("endpic/credit07.bmp", 0x1334B7, 19293, 0xC1E9DB91, CREDIT_HEADER),
    ("endpic/credit08.bmp", 0x13801F, 19293, 0xCBBCC7FA, CREDIT_HEADER),
    ("endpic/credit09.bmp", 0x13CB87, 19293, 0xFA7177B1, CREDIT_HEADER),
    ("endpic/credit10.bmp", 0x1416EF, 19293, 0x56390A07, CREDIT_HEADER),
    ("endpic/credit11.bmp", 0x146257, 19293, 0xFF3D6D83, CREDIT_HEADER),
    ("endpic/credit12.bmp", 0x14ADBF, 19293, 0x9E948DC2, CREDIT_HEADER),
    ("endpic/credit14.bmp", 0x14F927, 19293, 0x32B6CE2D, CREDIT_HEADER),
    ("endpic/credit15.bmp", 0x15448F, 19293, 0x88539803, CREDIT_HEADER),
    ("endpic/credit16.bmp", 0x158FF7, 19293, 0xC0EF9ADF, CREDIT_HEADER),
    ("endpic/credit17.bmp", 0x15DB5F, 19293, 0x8C5A003D, CREDIT_HEADER),
    ("endpic/credit18.bmp", 0x1626C7, 19293, 0x66BCBF22, CREDIT_HEADER),
    ("endpic/pixel.bmp", 0x16722F, 1373, 0x6181D0A1, PIXEL_HEADER),
    ("wavetable.dat", 0x110664, 25599, 0xCAA7B1DD, None),
    ("org/access.org", 0x09B35C, 1138, 0xD965DDDB, None),
    ("org/balcony.org", 0x09DBBC, 3082, 0x892345CA, None),
    ("org/balrog.org", 0x0B45A0, 5970, 0xB02093B8, None),
    ("org/breakdown.org", 0x09F5BC, 2570, 0xF80DD62A, None),
    ("org/cemetary.org", 0x09FFC8, 4578, 0x2CE377CC, None),
    ("org/charge.org", 0x0D28D4, 2770, 0x10DEC9D5, None),
    ("org/credits.org", 0x0A7EAC, 17898, 0xA9ED4834, None),
    ("org/egg.org", 0x0FEB20, 19626, 0xB651047E, None),
    ("org/eyesofflame.org", 0x0AEDC0, 21354, 0x6B5FF989, None),
    ("org/fanfale1.org", 0x0AE25C, 914, 0xAEFD547B, None),
    ("org/fanfale2.org", 0x0AE98C, 1074, 0x3A5170A6, None),
    ("org/fanfale3.org", 0x0AE5F0, 922, 0x85813929, None),
    ("org/gameover.org", 0x0B412C, 1137, 0x525D58F3, None),
    ("org/geothermal.org", 0x0B5CF4, 13466, 0xDB4795AC, None),
    ("org/gestation.org", 0x0F83C8, 10458, 0xCE2E68C1, None),
    ("org/gravity.org", 0x0B9190, 20578, 0x64A9318D, None),
    ("org/grasstown.org", 0x1037CC, 23706, 0xA27883B6, None),
    ("org/hell.org", 0x0BE1F4, 18386, 0x93BBF277, None),
    ("org/heroend.org", 0x0F1598, 9722, 0xFC64D0D0, None),
    ("org/jenka1.org", 0x0C5E54, 8306, 0xB42D7EAA, None),
    ("org/jenka2.org", 0x0C7EC8, 11986, 0xC095CBE1, None),
    ("org/labyrinth.org", 0x0DBCB8, 14786, 0x0292CF2C, None),
    ("org/lastbattle.org", 0x0CD650, 21122, 0x8888DAC9, None),
    ("org/lastcave.org", 0x0D33A8, 18122, 0x469B38B9, None),
    ("org/meltdown2.org", 0x0DF67C, 21074, 0x83D08AED, None),
    ("org/oppression.org", 0x0C29C8, 13450, 0x3CE4CDBE, None),
    ("org/oside.org", 0x0E725C, 25634, 0x1E33B095, None),
    ("org/plant.org", 0x0ED680, 11378, 0x3911E040, None),
    ("org/pulse.org", 0x0CAD9C, 10418, 0x92EF0330, None),
    ("org/quiet.org", 0x0F02F4, 4770, 0x0E95A468, None),
    ("org/run.org", 0x0AC498, 7618, 0x65A4BB85, None),
    ("org/safety.org", 0x09B7D0, 9194, 0x779E83C2, None),
    ("org/scorching.org", 0x0FACA4, 15994, 0xD09341E2, None),
    ("org/seal.org", 0x09E7C8, 3570, 0x373988AD, None),
    ("org/theme.org", 0x0A11AC, 25738, 0xF5ACE8B0, None),
    ("org/toroko.org", 0x0F3B94, 18482, 0xC202DE07, None),
    ("org/town.org", 0x0E48D0, 10634, 0x6A6AA627, None),
    ("org/tyrant.org", 0x0A7638, 2162, 0xC64DC450, None),
    ("org/waterway.org", 0x0D7A74, 16962, 0xB533D72A, None),
    ("org/white.org", 0x109468, 23714, 0xCFF0FB34, None),
    ("org/zombie.org", 0x10F180, 5346, 0xD217CC29, None),
]

PXT_FIELDS = [
    ("use  ", True),
    ("size ", True),
    ("main_model   ", True),
    ("main_freq    ", False),
    ("main_top     ", True),
    ("main_offset  ", True),
    ("pitch_model  ", True),
    ("pitch_freq   ", False),
    ("pitch_top    ", True),
    ("pitch_offset ", True),
    ("volume_model ", True),
    ("volume_freq  ", False),
    ("volume_top   ", True),
    ("volume_offset", True),
    ("initialY", True),
    ("ax      ", True),
    ("ay      ", True),
    ("bx      ", True),
    ("by      ", True),
    ("cx      ", True),
    ("cy      ", True),
]

# (id, nchanl, offset) — matches extractpxt.cpp (including duplicate 0x68).
PXT_SOUNDS = [
    (0x01, 1, 0x0907B0), (0x02, 1, 0x0909E0), (0x03, 1, 0x0934C0),
    (0x04, 1, 0x090890), (0x05, 1, 0x090660), (0x06, 1, 0x093530),
    (0x07, 1, 0x0935A0), (0x0B, 1, 0x090740), (0x0C, 2, 0x090C80),
    (0x0E, 1, 0x090A50), (0x0F, 1, 0x08FBE0), (0x10, 2, 0x090350),
    (0x11, 3, 0x090430), (0x12, 1, 0x090820), (0x14, 2, 0x090900),
    (0x15, 1, 0x090C10), (0x16, 1, 0x0906D0), (0x17, 1, 0x08FCC0),
    (0x18, 1, 0x08FC50), (0x19, 2, 0x090D60), (0x1A, 2, 0x090B30),
    (0x1B, 1, 0x090E40), (0x1C, 2, 0x0910E0), (0x1D, 1, 0x0911C0),
    (0x1E, 1, 0x091EE0), (0x1F, 1, 0x091310), (0x20, 2, 0x08F940),
    (0x21, 2, 0x08FA20), (0x22, 2, 0x08FB00), (0x23, 3, 0x090EB0),
    (0x25, 2, 0x092810), (0x26, 2, 0x091230), (0x27, 3, 0x091000),
    (0x28, 2, 0x092730), (0x29, 2, 0x092730), (0x2A, 1, 0x091380),
    (0x2B, 1, 0x0913F0), (0x2C, 3, 0x091460), (0x2D, 1, 0x0915B0),
    (0x2E, 1, 0x091620), (0x2F, 1, 0x091700), (0x30, 1, 0x091770),
    (0x31, 2, 0x0917E0), (0x32, 2, 0x08FD30), (0x33, 2, 0x08FE10),
    (0x34, 2, 0x08FEF0), (0x35, 2, 0x090580), (0x36, 2, 0x091A80),
    (0x37, 2, 0x092EA0), (0x38, 2, 0x092650), (0x39, 2, 0x0928F0),
    (0x3A, 2, 0x092DC0), (0x3B, 1, 0x093060), (0x3C, 1, 0x0930D0),
    (0x3D, 1, 0x093140), (0x3E, 2, 0x0931B0), (0x3F, 2, 0x093290),
    (0x40, 2, 0x093370), (0x41, 1, 0x093450), (0x46, 2, 0x08FFD0),
    (0x47, 2, 0x0900B0), (0x48, 2, 0x090190), (0x64, 1, 0x0918C0),
    (0x65, 3, 0x091930), (0x66, 2, 0x091B60), (0x67, 2, 0x091C40),
    (0x68, 1, 0x091CB0), (0x68, 1, 0x092C00), (0x69, 1, 0x091D20),
    (0x6A, 2, 0x091D90), (0x6B, 1, 0x091E70), (0x6C, 1, 0x091F50),
    (0x6D, 1, 0x091FC0), (0x6E, 1, 0x092030), (0x6F, 1, 0x0920A0),
    (0x70, 1, 0x092110), (0x71, 1, 0x092180), (0x72, 2, 0x0921F0),
    (0x73, 3, 0x092AB0), (0x74, 3, 0x092C70), (0x75, 2, 0x092F80),
    (0x96, 2, 0x0922D0), (0x97, 2, 0x0923B0), (0x98, 1, 0x092490),
    (0x99, 1, 0x092500), (0x9A, 2, 0x092570), (0x9B, 2, 0x0929D0),
]

NMAPS = 95
STAGE_DATA_OFFSET = 0x937B0
EXE_MAP_REC = 200  # sizeof(EXEMapRecord) on MSVC freeware build

BACKDROP_NAMES = [
    "bk0", "bkBlue", "bkGreen", "bkBlack", "bkGard", "bkMaze",
    "bkGray", "bkRed", "bkWater", "bkMoon", "bkFog", "bkFall",
]
TILESET_NAMES = [
    "0", "Pens", "Eggs", "EggX", "EggIn", "Store", "Weed", "Barr", "Maze",
    "Sand", "Mimi", "Cave", "River", "Gard", "Almond", "Oside", "Cent",
    "Jail", "White", "Fall", "Hell", "Labo",
]
NPCSET_NAMES = [
    "guest", "0", "eggs1", "ravil", "weed", "maze", "sand", "omg", "cemet",
    "bllg", "plant", "frog", "curly", "stream", "ironh", "toro", "x", "dark",
    "almo1", "eggs2", "twind", "moon", "cent", "heri", "red", "miza", "dr",
    "almo2", "kings", "hell", "press", "priest", "ballos", "island",
]


def crc32_ieee(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def cstr(buf: bytes) -> str:
    return buf.split(b"\0", 1)[0].decode("ascii", errors="replace")


def find_index(name: str, names: list[str]) -> int:
    low = name.lower()
    for i, n in enumerate(names):
        if n.lower() == low:
            return i
    return 0xFF


def extract_files(exe: bytes, outdir: Path, *, check_crc: bool = True) -> None:
    for rel, offset, length, expect_crc, header in FILES:
        chunk = exe[offset : offset + length]
        if len(chunk) != length:
            raise SystemExit(f"short read for {rel} at 0x{offset:x}")
        if check_crc:
            got = crc32_ieee(chunk)
            if got != expect_crc:
                raise SystemExit(
                    f"CRC mismatch for {rel}: got 0x{got:08x}, expected 0x{expect_crc:08x} "
                    "(need freeware Doukutsu.exe 1.0.0.6 / AGTP English)"
                )
        blob = (header + chunk) if header else chunk
        path = outdir / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(blob)
        print(f"  [file] {rel} ({len(blob)} bytes)")


def read_pxt_value(exe: bytes, pos: int, is_int: bool) -> tuple[object, int]:
    if is_int:
        (val,) = struct.unpack_from("<i", exe, pos)
        return val, pos + 4
    # fgetfloat: 4 padding bytes + 8-byte host double
    (val,) = struct.unpack_from("<d", exe, pos + 4)
    return val, pos + 12


def extract_pxt(exe: bytes, outdir: Path) -> None:
    pxt_dir = outdir / "pxt"
    pxt_dir.mkdir(parents=True, exist_ok=True)
    for sid, nchanl, offset in PXT_SOUNDS:
        pos = offset
        channels: list[list[object]] = []
        for _c in range(nchanl):
            values: list[object] = []
            for _name, is_int in PXT_FIELDS:
                val, pos = read_pxt_value(exe, pos, is_int)
                values.append(val)
            (pad,) = struct.unpack_from("<i", exe, pos)
            pos += 4
            if pad != 0:
                raise SystemExit(f"PXT out of sync at id=0x{sid:02x} offset=0x{offset:x}")
            channels.append(values)
        # Pad to 4 channels of zeros like the C writer.
        while len(channels) < 4:
            channels.append([0 if is_int else 0.0 for _n, is_int in PXT_FIELDS])

        lines: list[str] = []
        for values in channels:
            for (name, is_int), val in zip(PXT_FIELDS, values):
                if is_int:
                    lines.append(f"{name}:{int(val)}\r\n")
                else:
                    lines.append(f"{name}:{float(val):.2f}\r\n")
            lines.append("\r\n")
        for values in channels:
            parts: list[str] = []
            for (_name, is_int), val in zip(PXT_FIELDS, values):
                if is_int:
                    parts.append(str(int(val)))
                else:
                    parts.append(f"{float(val):.2f}")
            lines.append("{" + ",".join(parts) + "},\r\n")

        out = pxt_dir / f"fx{sid:02x}.pxt"
        out.write_bytes("".join(lines).encode("ascii"))
        print(f"  [pxt ] {out.relative_to(outdir)}")


def extract_stages(exe: bytes, outdir: Path) -> None:
    raw = exe[STAGE_DATA_OFFSET : STAGE_DATA_OFFSET + NMAPS * EXE_MAP_REC]
    if len(raw) != NMAPS * EXE_MAP_REC:
        raise SystemExit("short read for stage table")

    records = bytearray()
    records.append(NMAPS)
    for i in range(NMAPS):
        base = i * EXE_MAP_REC
        tileset = cstr(raw[base : base + 32])
        filename = cstr(raw[base + 32 : base + 64])
        (scroll_type,) = struct.unpack_from("<i", raw, base + 64)
        background = cstr(raw[base + 68 : base + 100])
        npc1 = cstr(raw[base + 100 : base + 132])
        npc2 = cstr(raw[base + 132 : base + 164])
        boss_no = raw[base + 164]
        caption = cstr(raw[base + 165 : base + 200])

        ti = find_index(tileset, TILESET_NAMES)
        bi = find_index(background, BACKDROP_NAMES)
        n1 = find_index(npc1, NPCSET_NAMES)
        n2 = find_index(npc2, NPCSET_NAMES)
        if 0xFF in (ti, bi, n1, n2):
            raise SystemExit(
                f"unrecognized stage field on map {i}: "
                f"tileset={tileset!r} bg={background!r} npc1={npc1!r} npc2={npc2!r}"
            )

        rec = bytearray(73)
        fb = filename.encode("ascii", errors="replace")[:31]
        sb = caption.encode("ascii", errors="replace")[:34]
        rec[0 : len(fb)] = fb
        rec[32 : 32 + len(sb)] = sb
        rec[67] = ti
        rec[68] = bi
        rec[69] = scroll_type & 0xFF
        rec[70] = boss_no
        rec[71] = n1
        rec[72] = n2
        records += rec

    out = outdir / "stage.dat"
    out.write_bytes(bytes(records))
    print(f"  [stage] stage.dat ({len(records)} bytes, {NMAPS} maps)")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("exe", type=Path, help="path to Doukutsu.exe")
    ap.add_argument(
        "-C",
        "--outdir",
        type=Path,
        default=None,
        help="output directory (default: directory containing the exe)",
    )
    ap.add_argument("--no-crc", action="store_true", help="skip CRC checks")
    args = ap.parse_args()

    exe_path = args.exe
    if not exe_path.is_file():
        print(f"error: missing {exe_path}", file=sys.stderr)
        return 1
    outdir = args.outdir or exe_path.parent
    outdir.mkdir(parents=True, exist_ok=True)

    exe = exe_path.read_bytes()
    print(f"extracting from {exe_path} → {outdir}")
    extract_files(exe, outdir, check_crc=not args.no_crc)
    extract_pxt(exe, outdir)
    extract_stages(exe, outdir)
    print("extract complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
