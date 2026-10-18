import re
import struct

here = 'tools/gow3_ref'
data = open('out/p3d_test/boot.bin', 'rb').read()
size, entry, ns, nr, ni, caps = struct.unpack_from('<QQQQQQ', data, 8)
pos = 56 + ns * 24
names = [data[pos + i * 128:pos + (i + 1) * 128].split(b'\0')[0].decode() for i in range(ni)]
inc_text = open(here + '/src/import_names.inc').read()
symbols = {k.split('#')[0]: (k, v) for k, v in re.findall(r'\{"([^"]+)","([^"]+)"\}', inc_text)}

known = [n for n in names if n.split('#')[0] in symbols]
unknown = [n for n in names if n.split('#')[0] not in symbols]

print(f"Total imports in Persona 3 Dancing: {len(names)}")
print(f"Recognized in gow3_ref import_names.inc: {len(known)} / {len(names)}")
print(f"Unknown / not in gow3_ref symbol table: {len(unknown)}")
print("\nSample recognized imports:")
for n in known[:30]:
    print(f"  {n:<20} -> {symbols[n.split('#')[0]][1]} ({symbols[n.split('#')[0]][0]})")

print("\nSample unrecognized NIDs:")
for n in unknown[:15]:
    print(f"  {n}")
