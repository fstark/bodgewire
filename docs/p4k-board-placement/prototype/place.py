# Places the chips of boards/tennis/tennis.net on a grid by simulated annealing (half-perimeter
# wire length, VCC/GND left out); everything that is not a DIP stays where the Board has it.
# usage: place.py COLS ROWS XPITCH YPITCH X0 Y0 FIXED_JSON SEED OUT_JSON
import re,random,math,sys,json,collections
net=open('boards/tennis/tennis.net').read()
fp={m[0]:m[1] for m in re.findall(r'\(comp \(ref "([^"]+)"\) \(value "[^"]*"\) \(footprint "([^"]+)"',net)}
nets={}
for chunk in net.split('(net (code')[1:]:
    name=re.search(r'\(name "([^"]+)"\)',chunk).group(1)
    nodes=re.findall(r'\(ref "([^"]+)"\) \(pin "([^"]+)"\)',chunk)
    if name in ('VCC','GND'): continue
    if len(set(r for r,_ in nodes))>1: nets[name]=nodes
print(len(nets),"nets")
def npins(r):
    m=re.search(r'DIP-(\d+)',fp[r]); return int(m.group(1)) if m else 0
def off(r,p):
    n=npins(r)
    if not n: return (0.0,0.0)
    p=int(p)
    return (0.0,(p-1)*2.54) if p<=n//2 else (7.62,(n-p)*2.54)
pcb=open('boards/tennis/tennis.kicad_pcb').read()
cur={}
for m in re.finditer(r'\(footprint "([^"]+)"',pcb):
    seg=pcb[m.start():m.start()+3000]
    at=re.search(r'\(at ([-\d.]+) ([-\d.]+)',seg); ref=re.search(r'\(property "Reference" "([^"]+)"',seg).group(1)
    cur[ref]=(float(at.group(1)),float(at.group(2)))
chips=sorted([r for r in fp if npins(r)],key=lambda r:(r[0],int(r[1:])))
fixed={r:cur[r] for r in fp if not npins(r)}
COLS,ROWS,XP,YP,X0,Y0=int(sys.argv[1]),int(sys.argv[2]),float(sys.argv[3]),float(sys.argv[4]),float(sys.argv[5]),float(sys.argv[6])
fixed.update(json.loads(sys.argv[7]) if len(sys.argv)>7 else {})
slots=[(X0+c*XP,Y0+r*YP) for r in range(ROWS) for c in range(COLS)]
def cost_net(pos,n):
    xs=[];ys=[]
    for r,p in nets[n]:
        x,y=pos[r]; dx,dy=off(r,p); xs.append(x+dx); ys.append(y+dy)
    return (max(xs)-min(xs))+(max(ys)-min(ys))
def total(pos): return sum(cost_net(pos,n) for n in nets)
pos=dict(cur); print("user placement HPWL: %.0f mm"%total(pos))
bynet=collections.defaultdict(list)
for n,nodes in nets.items():
    for r,_ in nodes: bynet[r].append(n)
random.seed(int(sys.argv[8]) if len(sys.argv)>8 else 1)
assign=list(range(len(slots))); random.shuffle(assign)
slot_of={c:assign[i] for i,c in enumerate(chips)}
occ={s:None for s in range(len(slots))}
for c,s in slot_of.items(): occ[s]=c
pos=dict(fixed); pos.update({c:slots[s] for c,s in slot_of.items()})
cn={n:cost_net(pos,n) for n in nets}; C=sum(cn.values())
T=200.0; N=600000
for it in range(N):
    T=200.0*(0.0005/200.0)**(it/N)
    a=random.choice(chips); sb=random.randrange(len(slots)); sa=slot_of[a]
    if sb==sa: continue
    b=occ[sb]
    touched=set(bynet[a])|(set(bynet[b]) if b else set())
    old=sum(cn[n] for n in touched)
    pos[a]=slots[sb]
    if b: pos[b]=slots[sa]
    new={n:cost_net(pos,n) for n in touched}; d=sum(new.values())-old
    if d<=0 or random.random()<math.exp(-d/T):
        slot_of[a]=sb; occ[sb]=a; occ[sa]=b
        if b: slot_of[b]=sa
        cn.update(new); C+=d
    else:
        pos[a]=slots[sa]
        if b: pos[b]=slots[sb]
print("optimised HPWL: %.0f mm"%total(pos))
for r in range(ROWS):
    print(" ".join(f"{(occ[r*COLS+c] or '--'):>4}" for c in range(COLS)))
json.dump({c:slots[s] for c,s in slot_of.items()},open(sys.argv[9] if len(sys.argv)>9 else '/dev/null','w'))
