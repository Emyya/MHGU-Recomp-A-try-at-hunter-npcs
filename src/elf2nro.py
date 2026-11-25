"""ELF (AArch32, linked with switch32.ld) -> NRO, the same conversion as forge-rs's cargo-forge (src/nro.rs)."""
import struct, sys

PAGE = 0x1000
def align(v, a=PAGE): return (v + a - 1) & ~(a - 1)

elf = open(sys.argv[1], 'rb').read()
assert elf[:4] == b'\x7fELF' and elf[4] == 1 and elf[5] == 1
phoff, = struct.unpack_from('<I', elf, 28)
phnum, = struct.unpack_from('<H', elf, 44)
loads = []
for k in range(phnum):
    t, off, va, pa, fsz, msz = struct.unpack_from('<IIIIII', elf, phoff + 32 * k)
    if t == 1: loads.append((off, va, fsz, msz))
assert len(loads) >= 3, loads
seg_off, seg_size, total = [], [], 0
for off, va, fsz, msz in loads[:3]:
    size = align(fsz); seg_off.append(va); seg_size.append(size); total = align(total + size)
d = loads[2]
bss = align(d[3] - align(d[2])) if d[3] > align(d[2]) else 0
# build id
build_id = b'\0' * 0x20
shoff, = struct.unpack_from('<I', elf, 32); shentsize, shnum = struct.unpack_from('<HH', elf, 46)
for k in range(shnum):
    sh = shoff + k * shentsize
    if struct.unpack_from('<I', elf, sh + 4)[0] != 7: continue
    no = struct.unpack_from('<I', elf, sh + 16)[0]
    nsz, dsz, nt = struct.unpack_from('<III', elf, no)
    if nt == 3 and elf[no + 12:no + 16] == b'GNU\0':
        build_id = (elf[no + 12 + nsz:no + 12 + nsz + min(dsz, 0x20)] + b'\0' * 0x20)[:0x20]
img = bytearray(total)
for i, (off, va, fsz, msz) in enumerate(loads[:3]):
    img[va:va + fsz] = elf[off:off + fsz]
hdr = bytearray(0x70)
hdr[0:4] = b'NRO0'
struct.pack_into('<I', hdr, 8, total)
for i in range(3): struct.pack_into('<II', hdr, 0x10 + 8 * i, seg_off[i], seg_size[i])
struct.pack_into('<I', hdr, 0x28, bss)
hdr[0x30:0x50] = build_id
img[0x10:0x80] = hdr
open(sys.argv[2], 'wb').write(img)
print('nro: %d bytes, segments %s, bss %#x' % (total, [(hex(a), hex(b)) for a, b in zip(seg_off, seg_size)], bss))
