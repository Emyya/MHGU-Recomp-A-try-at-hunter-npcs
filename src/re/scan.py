import os
TOOLS = os.environ.get('MHGU_TOOLS', 'tools')        # MHGURecomp's tools/ folder (nso.py)
MAIN = os.environ.get('MHGU_MAIN', 'main')          # the game's exefs/main
CACHE = os.environ.get('MHGU_RE_CACHE', 'text.pkl')  # scan.py's output
import sys, struct, pickle, os, capstone
sys.path.insert(0,TOOLS)
from nso import Nso
n=Nso(MAIN); img=n.image
if not os.path.exists(CACHE):
    md=capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM); md.skipdata=True
    text=n.text if isinstance(n.text, tuple) else None
    print('text', text, file=sys.stderr)
    off, size = text[0], text[1] if text else (0, 0)
    out=[]
    for i in md.disasm_lite(bytes(img[off:off+size]), off+0x200000):
        out.append((i[0], i[2], i[3]))
    pickle.dump(out, open(CACHE,'wb'))
