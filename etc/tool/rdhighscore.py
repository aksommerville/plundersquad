#!/usr/bin/env python
import sys,os

if len(sys.argv)!=2:
  sys.stderr.write("Usage: %s HIGHSCORES\n"%(sys.argv[0],))
  sys.exit(1)

count_by_plrdefid={}

def read_multibyte(src):
  """High scores file is written directly from C structs.
     So its byte order is system-dependant.
  """
  dst=0
  if True: # little-endian
    offset=0
    for ch in src:
      dst|=ord(ch)<<offset
      offset+=8
  else: # big-endian
    for ch in src:
      dst<<=8
      dst|=ord(ch)
  return dst

f=open(sys.argv[1],'r')
recordix=0
while True:
  record=f.read(40)
  if not record: break

  playerc=ord(record[8])
  if playerc>8:
    sys.stderr.write("%s: Illegal player count %d in record %d.\n"%(sys.argv[1],playerc,recordix))
    sys.exit(1)
  plrdefidv=map(ord,record[9:9+playerc])
  playtime=read_multibyte(record[20:24])
  difficulty=ord(record[17])
  length=ord(record[18])

  #sys.stdout.write("%4d: %r playtime=%d difficulty=%d length=%d\n"%(recordix,plrdefidv,playtime,difficulty,length))
  plrdefid_list=','.join(map(str,plrdefidv))
  sys.stdout.write("  PS_ASSERT_CALL(ps_score_store_add_false_record(store,%5d,%d,%d,%s,-1))\n"%(playtime,length,difficulty,plrdefid_list))

  for plrdefid in plrdefidv:
    if not (plrdefid in count_by_plrdefid):
      count_by_plrdefid[plrdefid]=1
    else:
      count_by_plrdefid[plrdefid]+=1
  recordix+=1

#for plrdefid,count in count_by_plrdefid.iteritems():
#  sys.stdout.write("plrdefid:%d: %d\n"%(plrdefid,count))
