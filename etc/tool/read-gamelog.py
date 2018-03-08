#!/usr/bin/env python
import sys,os

def bits_as_flags(src):
  src=ord(src)
  dst=[]
  for i in xrange(8):
    dst.append("X" if src&(1<<i) else ".")
  return " ".join(dst)

bpusage=open("etc/playtest/gamelog","r").read()
sys.stdout.write("      1 2 3 4 5 6 7 8\n")
for bpid,usage in enumerate(bpusage):
  sys.stdout.write("%4d: %s\n"%(bpid,bits_as_flags(usage)))
