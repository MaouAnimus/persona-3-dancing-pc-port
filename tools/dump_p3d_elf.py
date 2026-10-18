import struct
from pathlib import Path

elf_path = Path(r"c:\Users\Alvaro\Downloads\Persona 3 PC\CUSA12636\eboot.elf")
data = elf_path.read_bytes()

# ELF Header
magic = data[:4]
assert magic == b"\x7fELF"

phoff = struct.unpack_from("<Q", data, 32)[0]
phentsize = struct.unpack_from("<H", data, 54)[0]
phnum = struct.unpack_from("<H", data, 56)[0]

print(f"Program headers: {phnum} entries at offset 0x{phoff:x}")

dyn_offset = None
dyn_size = None
lib_offset = None
lib_size = None

for i in range(phnum):
    pos = phoff + i * phentsize
    p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from("<IIQQQQQQ", data, pos)
    if p_type == 2: # PT_DYNAMIC
        dyn_offset = p_offset
        dyn_size = p_filesz
        print(f"PT_DYNAMIC: offset=0x{dyn_offset:x}, size=0x{dyn_size:x}")
    elif p_type == 0x61000000: # PT_SCE_DYNLIBDATA
        lib_offset = p_offset
        lib_size = p_filesz
        print(f"PT_SCE_DYNLIBDATA: offset=0x{lib_offset:x}, size=0x{lib_size:x}")

dyn_tags = {}
if dyn_offset and dyn_size:
    for pos in range(dyn_offset, dyn_offset + dyn_size, 16):
        tag, val = struct.unpack_from("<QQ", data, pos)
        if tag == 0:
            break
        dyn_tags[tag] = val

print(f"Dynamic tags count: {len(dyn_tags)}")

# Check SCE dynamic tags
# 0x61000035 = DT_SCE_STRTAB, 0x61000037 = DT_SCE_STRSZ
# 0x61000039 = DT_SCE_SYMTAB, 0x6100003f = DT_SCE_SYMTABSZ
# 0x61000015 = DT_SCE_NEEDED_MODULE
blob = data[lib_offset:lib_offset + lib_size] if lib_offset else b""

if 0x61000035 in dyn_tags and 0x61000037 in dyn_tags and blob:
    str_off = dyn_tags[0x61000035]
    str_sz = dyn_tags[0x61000037]
    strings = blob[str_off:str_off + str_sz]
    
    needed_modules = []
    # Search for needed module tags or iterate through tags
    for pos in range(dyn_offset, dyn_offset + dyn_size, 16):
        tag, val = struct.unpack_from("<QQ", data, pos)
        if tag == 0x61000015: # DT_SCE_NEEDED_MODULE
            # val contains offset into strings or module info
            pass
            
    # Let's extract all readable ASCII strings from strings table
    all_strs = []
    cur = []
    for b in strings:
        if 32 <= b < 127:
            cur.append(chr(b))
        elif b == 0:
            if cur:
                all_strs.append("".join(cur))
                cur = []
    
    print(f"\nExtracted {len(all_strs)} strings from SCE string table.")
    module_names = [s for s in all_strs if s.startswith("lib") or "Sce" in s or ".prx" in s or s in ("libc", "libkernel")]
    print(f"Modules detected ({len(module_names)}):")
    for m in sorted(list(set(module_names))):
        print(f"  {m}")
