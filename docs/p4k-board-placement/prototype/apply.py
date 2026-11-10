# Copies a .kicad_pcb with footprints moved to a placement JSON and every track, via and arc removed.
# usage: apply.py SRC.kicad_pcb PLACEMENT.json OUT.kicad_pcb EXTRA_JSON
import re,json,sys
src,placement,out=sys.argv[1],sys.argv[2],sys.argv[3]
place=json.load(open(placement)); place.update(json.loads(sys.argv[4]))
t=open(src).read()
def blocks(t):
    res=[];depth=0;j=t.index('(',1);instr=False;start=None
    while j<len(t):
        ch=t[j]
        if instr:
            if ch=='\\': j+=2; continue
            if ch=='"': instr=False
        elif ch=='"': instr=True
        elif ch=='(':
            if depth==0: start=j
            depth+=1
        elif ch==')':
            if depth==0: break
            depth-=1
            if depth==0: res.append((start,j+1))
        j+=1
    return res
parts=[];last=0;moved=0;dropped=0
for s,e in blocks(t):
    b=t[s:e]
    head=re.match(r'\((\w+)',b).group(1)
    if head in('segment','via','arc'):
        parts.append(t[last:s]); last=e; dropped+=1; continue
    if head=='footprint':
        ref=re.search(r'\(property "Reference" "([^"]+)"',b).group(1)
        if ref in place:
            x,y=place[ref]
            m=re.search(r'\(at [-\d.]+ [-\d.]+',b)
            b=b[:m.start()]+f'(at {x:.3f} {y:.3f}'+b[m.end():]
            parts.append(t[last:s]); parts.append(b); last=e; moved+=1
parts.append(t[last:])
open(out,'w').write(''.join(parts))
print("moved",moved,"dropped",dropped)
