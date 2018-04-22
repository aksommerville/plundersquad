#!/usr/bin/env python
import sys,os

if len(sys.argv)!=2:
  sys.stderr.write("Usage: %s HIGHSCORES\n"%(sys.argv[0],))
  sys.exit(1)

count_by_plrdefid={}

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
  sys.stdout.write("%4d: %r\n"%(recordix,plrdefidv))
  for plrdefid in plrdefidv:
    if not (plrdefid in count_by_plrdefid):
      count_by_plrdefid[plrdefid]=1
    else:
      count_by_plrdefid[plrdefid]+=1
  recordix+=1

for plrdefid,count in count_by_plrdefid.iteritems():
  sys.stdout.write("plrdefid:%d: %d\n"%(plrdefid,count))
