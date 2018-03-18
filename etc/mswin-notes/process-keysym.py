#!/usr/bin/env/python
import sys

def report_keysym(symbol,value,desc):
  sys.stdout.write("%s %s %s\n"%(symbol,value,desc))
  #sys.stdout.write("    case %s: return 0x000700\n"%(symbol,))

# Sample:
#<tr><td data-th="Constant/value"><a id="VK_LBUTTON"><!----></a><a id="vk_lbutton"><!----></a><dl>
#<dt><strong>VK_LBUTTON</strong></dt>
#<dt>0x01</dt>
#</dl>
#</td><td data-th="Description">
#<p>Left mouse button</p>
#</td></tr>

def parse_symbol(src):
  startp=src.find("<strong>")
  if startp<0: return "?"
  endp=src.find("</strong>")
  if endp<0: return "?"
  symbol=src[startp+8:endp].strip()
  if not len(symbol): return "?"
  return symbol

def parse_value(src):
  startp=src.find("0x")
  if startp<0: return "?"
  endp=src.find("<",startp)
  if endp<0: return "?"
  return src[startp:endp]

def parse_desc(src):
  startp=src.find("<p>")
  if startp<0: return ""
  startp+=3
  endp=src.find("</p>",startp)
  if endp<0: return ""
  return src[startp:endp].strip()

def process_line(src):
  symbol=parse_symbol(src)
  value=parse_value(src)
  desc=parse_desc(src)

  if symbol=="?":
    sys.stderr.write("Failed to parse row:\n%s\n"%(src,))
    return
  if symbol=="-":
    if desc=="Unassigned": return
    if desc=="Reserved": return
    if desc=="Undefined": return
    sys.stderr.write("Failed to parse row:\n%s\n"%(src,))
    return
  if symbol=="<!---->":
    words=desc.split()
    if len(words)==2 and words[1]=="key":
      ch=chr(int(value,16))
      if ch==words[0]: return
      sys.stderr.write("Failed to parse row:\n%s\n"%(src,))
      return
    elif desc=="OEM specific":
      return
    else:
      sys.stderr.write("Failed to parse row:\n%s\n"%(src,))
      return
  if symbol=="VK_PACKET":
    # This is a very special key that we aren't going to handle, and it has a mile-long description. Drop it.
    return
  if symbol=="VK_HANGUEL":
    # Deprecated synonym for VK_HANGUL. Drop it.
    return

  value=int(value,16)

  report_keysym(symbol,value,desc)

src=sys.stdin.read()
srcp=0
while True:
  startp=src.find("<tr>",srcp)
  if startp<0: break
  startp+=4
  endp=src.find("</tr>",startp)
  if endp<0: break
  srcp=endp+5
  process_line(src[startp:endp])
