import os
TOOLS = os.environ.get('MHGU_TOOLS', 'tools')        # MHGURecomp's tools/ folder (nso.py)
MAIN = os.environ.get('MHGU_MAIN', 'main')          # the game's exefs/main
CACHE = os.environ.get('MHGU_RE_CACHE', 'text.pkl')  # scan.py's output
import pickle, re, sys
ins = pickle.load(open(CACHE,'rb'))
pat = re.compile(sys.argv[1])
lim = int(sys.argv[2]) if len(sys.argv) > 2 else 200
c = 0
for a, m, o in ins:
    s = '%s %s' % (m, o)
    if pat.search(s):
        print('%08x: %s' % (a, s)); c += 1
        if c >= lim: break
