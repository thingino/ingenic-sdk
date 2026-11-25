#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
retarget.py IN OUT NAME[=NEWSYM] ...

Route calls to functions of the vendor firmware object to C replacements.

A global NAME is turned into an undefined symbol of the same name, so the
module's own definition of it satisfies every reference.

A static NAME is called through relocations against the object's own .text
section, which no symbol rewrite reaches: each relocation that resolves to it
(a call or address load, found through the section-relative addend) is turned
into a relocation against the new undefined global NEWSYM with a zero addend.

Only symbol and relocation entries and the addend fields they own are touched;
no instruction is added, removed or reordered.
"""
import struct
import sys

R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16 = 4, 5, 6


def sx16(v):
    return v - 0x10000 if v & 0x8000 else v


def main():
    src, dst = sys.argv[1], sys.argv[2]
    want = {}
    for a in sys.argv[3:]:
        n, _, s = a.partition("=")
        want[n] = s
    b = bytearray(open(src, "rb").read())
    assert b[:4] == b"\x7fELF" and b[4] == 1 and b[5] == 1, "ELF32 LE only"
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", b, 0x2e)
    shs = [list(struct.unpack_from("<10I", b, shoff + i * shentsize)) for i in range(shnum)]

    def cstr(tab, off):
        s = tab[4] + off
        return bytes(b[s:b.index(0, s)]).decode()

    names = [cstr(shs[shstrndx], s[0]) for s in shs]
    sym_i = names.index(".symtab")
    symtab, strtab = shs[sym_i], shs[shs[sym_i][6]]
    nsym = symtab[5] // 16
    text_i = names.index(".text")
    text, rel = shs[text_i], shs[names.index(".rel.text")]

    secsym = None
    glob, local = {}, {}
    for i in range(nsym):
        n, v, sz, info, oth, shn = struct.unpack_from("<IIIBBH", b, symtab[4] + 16 * i)
        nm = cstr(strtab, n)
        if info & 0xf == 3 and shn == text_i:
            secsym = i
        if nm in want and info & 0xf == 2 and shn == text_i:
            if info >> 4 == 0:
                local[v] = nm
            else:
                glob[i] = nm
    assert secsym is not None and len(local) + len(glob) == len(want), (local, glob, want)

    for i in glob:
        o = symtab[4] + 16 * i
        n, v, sz, info, oth, shn = struct.unpack_from("<IIIBBH", b, o)
        struct.pack_into("<IIIBBH", b, o, n, 0, 0, (info >> 4) << 4, oth, 0)

    stab = bytearray(b[strtab[4]:strtab[4] + strtab[5]])
    ntab = bytearray(b[symtab[4]:symtab[4] + symtab[5]])
    new = {}
    for v, nm in local.items():
        new[v] = len(ntab) // 16
        ntab += struct.pack("<IIIBBH", len(stab), 0, 0, 0x10, 0, 0)
        stab += want[nm].encode() + b"\0"

    ents = [list(struct.unpack_from("<II", b, rel[4] + 8 * i)) for i in range(rel[5] // 8)]

    def word(off):
        return struct.unpack_from("<I", b, text[4] + off)[0]

    def put(off, w):
        struct.pack_into("<I", b, text[4] + off, w)

    sites = []
    hits = dict.fromkeys(local, 0)
    for k, (off, info) in enumerate(ents):
        if info >> 8 != secsym:
            continue
        t = info & 0xff
        if t == R_MIPS_26:
            a = (word(off) & 0x3ffffff) << 2
            if a in local:
                sites.append((a, [k]))
                hits[a] += 1
        elif t == R_MIPS_HI16:
            for j in range(k + 1, len(ents)):
                if ents[j][1] & 0xff == R_MIPS_LO16 and ents[j][1] >> 8 == secsym:
                    a = (((word(off) & 0xffff) << 16) + sx16(word(ents[j][0]) & 0xffff)) & 0xffffffff
                    if a in local:
                        sites.append((a, [k, j]))
                        hits[a] += 1
                    break
    for a, nm in local.items():
        print("%s @0x%x: %d site(s)" % (nm, a, hits[a]), file=sys.stderr)
        assert hits[a], nm
    for a, idxs in sites:
        for k in idxs:
            off, info = ents[k]
            t = info & 0xff
            ents[k][1] = (new[a] << 8) | t
            put(off, word(off) & (~0x3ffffff if t == R_MIPS_26 else ~0xffff))
    for i, (off, info) in enumerate(ents):
        struct.pack_into("<II", b, rel[4] + 8 * i, off, info)

    def append(data):
        while len(b) % 4:
            b.append(0)
        o = len(b)
        b.extend(data)
        return o

    symtab[4], symtab[5] = append(ntab), len(ntab)
    strtab[4], strtab[5] = append(stab), len(stab)
    for idx, s in ((sym_i, symtab), (symtab[6], strtab)):
        struct.pack_into("<10I", b, shoff + idx * shentsize, *s)
    open(dst, "wb").write(b)


main()
