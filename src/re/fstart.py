import os
TOOLS = os.environ.get('MHGU_TOOLS', 'tools')        # MHGURecomp's tools/ folder (nso.py)
MAIN = os.environ.get('MHGU_MAIN', 'main')          # the game's exefs/main
CACHE = os.environ.get('MHGU_RE_CACHE', 'text.pkl')  # scan.py's output
import sys,struct,capstone
sys.path.insert(0,TOOLS)
from nso import Nso
n=Nso(MAIN); img=n.image
md=capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
a=int(sys.argv[1],0)
while True:
    a-=4
    w=struct.unpack_from('<I',img,a-0x200000)[0]
    ins=list(md.disasm(struct.pack('<I',w),a))
    if ins and ins[0].mnemonic in('push','stmdb') and 'lr' in ins[0].op_str:
        print(hex(a)); break
