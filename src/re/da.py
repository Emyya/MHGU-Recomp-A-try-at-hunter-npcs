import os
TOOLS = os.environ.get('MHGU_TOOLS', 'tools')        # MHGURecomp's tools/ folder (nso.py)
MAIN = os.environ.get('MHGU_MAIN', 'main')          # the game's exefs/main
CACHE = os.environ.get('MHGU_RE_CACHE', 'text.pkl')  # scan.py's output
"""Disassembler with pc-relative literal / global resolution. usage: da.py <runtime addr> [count]"""
import re, struct, sys
sys.path.insert(0, TOOLS)
import capstone
from nso import Nso

BASE = 0x200000
n = Nso(MAIN)
img = n.image
KNOWN = {
    0x1a85b58: 'sPlayer*', 0x1a8ca48: 'sGameControl*', 0x1a51fe0: 'sEnemy*', 0x1a8ae38: 'sQuestSelector*',
}
# exported / imported symbol names
SYMS = {}
try:
    for name, addr, *_ in n.exports():
        SYMS[addr + BASE] = name
except Exception:
    pass


def u32(img_off):
    return struct.unpack_from('<I', img, img_off)[0]


def run(addr, count):
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    off = addr - BASE
    code = bytes(img[off:off + 4 * count])
    lit = {}  # reg -> literal value loaded from pool
    for ins in md.disasm(code, addr):
        note = ''
        m = re.match(r'(r\d+|sb|sl|fp|ip|lr), \[pc, #(-?0x[0-9a-f]+|-?\d+)\]$', ins.op_str)
        if ins.mnemonic == 'ldr' and m:
            pool = ins.address + 8 + int(m.group(2), 0)
            v = u32(pool - BASE)
            lit[m.group(1)] = v
            note = 'lit 0x%x' % v
        m2 = re.match(r'(r\d+|sb|sl|fp|ip|lr), \[pc, (r\d+|sb|sl|fp|ip|lr)\]$', ins.op_str)
        if ins.mnemonic == 'ldr' and m2 and m2.group(2) in lit:
            g = (ins.address + 8 + lit[m2.group(2)]) & 0xffffffff
            v = u32(g - BASE) if 0 <= g - BASE < len(img) - 4 else None
            name = KNOWN.get(g, '')
            note = 'load [0x%x] %s' % (g, name)
            if v is not None and not name:
                note += ' (=0x%x)' % v
        m3 = re.match(r'(r\d+|sb|sl|fp|ip|lr), pc, (r\d+|sb|sl|fp|ip|lr)$', ins.op_str)
        if ins.mnemonic == 'add' and m3 and m3.group(2) in lit:
            g = (ins.address + 8 + lit[m3.group(2)]) & 0xffffffff
            note = 'addr 0x%x %s' % (g, KNOWN.get(g, ''))
            try:
                s = n.cstr(g - BASE)
                if s and 3 <= len(s) < 80 and s.isprintable():
                    note += ' "%s"' % s
            except Exception:
                pass
        if ins.mnemonic in ('bl', 'blx', 'b') and ins.op_str.startswith('#'):
            t = int(ins.op_str[1:], 0)
            if t in SYMS:
                note = SYMS[t]
        print('%08x: %-6s %-34s %s' % (ins.address, ins.mnemonic, ins.op_str, ('; ' + note) if note else ''))
        if ins.mnemonic == 'pop' and 'pc' in ins.op_str and count < 0:
            break


if __name__ == '__main__':
    run(int(sys.argv[1], 0), int(sys.argv[2], 0) if len(sys.argv) > 2 else 64)
