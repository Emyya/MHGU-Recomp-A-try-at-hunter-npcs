Reverse-engineering tools used (Python 3, capstone, lz4; they need nso.py from MHGURecomp's tools/).
Set where things are with environment variables (defaults: the current folder):
  MHGU_TOOLS     MHGURecomp's tools/ folder
  MHGU_MAIN      the game's exefs/main (from your own copy of the game; never commit it)
  MHGU_RE_CACHE  where scan.py keeps its cache (text.pkl)
  scan.py      disassembles the whole .text once into the cache
  grep_ins.py  searches instructions by regex in that cache:  grep_ins.py '#0x25ac\]' 50
  da.py        disassembles from a runtime address, resolving literals / GOT:  da.py 0x4a79cc 60
  fstart.py    finds the start of the function holding an address:  fstart.py 0x49badc
