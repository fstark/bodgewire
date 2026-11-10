# Like apply.py, but the placement gives [x, y, angle]; the angle goes on the footprint and on every
# pad and text inside it, as KiCad writes them. Tracks, vias and arcs removed.
# usage: apply3.py SRC.kicad_pcb PLACEMENT.json OUT.kicad_pcb
import re,json,sys
src,placement,out=sys.argv[1:4]
place=json.load(open(placement))
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
def fmt(v): return f'{v:.3f}'.rstrip('0').rstrip('.')
parts=[];last=0
for s,e in blocks(t):
    b=t[s:e]; head=re.match(r'\((\w+)',b).group(1)
    if head in('segment','via','arc'):
        parts.append(t[last:s]); last=e; continue
    if head=='footprint':
        ref=re.search(r'\(property "Reference" "([^"]+)"',b).group(1)
        if ref in place:
            x,y,a=place[ref]
            m=re.search(r'\(at [-\d.]+ [-\d.]+(?: [-\d.]+)?\)',b)
            head_at=f'(at {fmt(x)} {fmt(y)}'+(f' {a})' if a else ')')
            rest=b[m.end():]
            def turn(q):
                if '(point' in rest[max(0,q.start()-40):q.start()]: return q.group(0)
                return f'(at {q.group(1)} {q.group(2)}'+(f' {a})' if a else ')')
            rest=re.sub(r'\(at ([-\d.]+) ([-\d.]+)(?: [-\d.]+)?\)',turn,rest)
            b=b[:m.start()]+head_at+rest
            parts.append(t[last:s]); parts.append(b); last=e
parts.append(t[last:])
open(out,'w').write(''.join(parts))
