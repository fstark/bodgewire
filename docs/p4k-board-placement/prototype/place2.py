# Like place.py, but connectors move too, each along its own edge; J3 kept left of J4.
# usage: place2.py SEED OUT_JSON
import re,random,math,sys,json,collections
net=open('boards/tennis/tennis.net').read()
fp={m[0]:m[1] for m in re.findall(r'\(comp \(ref "([^"]+)"\) \(value "[^"]*"\) \(footprint "([^"]+)"',net)}
nets={}
for chunk in net.split('(net (code')[1:]:
    name=re.search(r'\(name "([^"]+)"\)',chunk).group(1)
    nodes=re.findall(r'\(ref "([^"]+)"\) \(pin "([^"]+)"\)',chunk)
    if name in ('VCC','GND'): continue
    if len(set(r for r,_ in nodes))>1: nets[name]=nodes
def npins(r):
    m=re.search(r'DIP-(\d+)',fp[r]); return int(m.group(1)) if m else 0
def off(r,p):
    n=npins(r)
    if not n: return (0.0,0.0)
    p=int(p)
    return (0.0,(p-1)*2.54) if p<=n//2 else (7.62,(n-p)*2.54)
chips=sorted([r for r in fp if npins(r)],key=lambda r:(r[0],int(r[1:])))
COLS,ROWS,XP,YP,X0,Y0=12,5,16,26,45,28
groups={
 'chip':(chips,[(X0+c*XP,Y0+r*YP) for r in range(ROWS) for c in range(COLS)]),
 'back':(['J2'],[(30+i*14,19) for i in range(16)]),
 'left':(['J1'],[(24,30+i*12) for i in range(11)]),
 'front':(['J3','J4','SW1','R1'],[(30+i*14,157) for i in range(16)]),
}
def cost_net(pos,n):
    xs=[];ys=[]
    for r,p in nets[n]:
        x,y=pos[r]; dx,dy=off(r,p); xs.append(x+dx); ys.append(y+dy)
    return (max(xs)-min(xs))+(max(ys)-min(ys))
bynet=collections.defaultdict(list)
for n,nodes in nets.items():
    for r,_ in nodes: bynet[r].append(n)
random.seed(int(sys.argv[1]))
slot_of={};occ={};pos={}
for g,(items,slots) in groups.items():
    while True:
        pick=random.sample(range(len(slots)),len(items))
        if g!='front' or slots[pick[0]][0]<slots[pick[1]][0]: break
    for it,s in zip(items,pick): slot_of[it]=(g,s); occ[(g,s)]=it; pos[it]=slots[s]
movable=[it for g,(items,_) in groups.items() for it in items]
def ok(): return pos['J3'][0]<pos['J4'][0]
cn={n:cost_net(pos,n) for n in nets}
N=800000
for k in range(N):
    T=200.0*(0.0005/200.0)**(k/N)
    a=random.choice(movable); g,sa=slot_of[a]; slots=groups[g][1]
    sb=random.randrange(len(slots))
    if sb==sa: continue
    b=occ.get((g,sb))
    pos[a]=slots[sb]
    if b: pos[b]=slots[sa]
    if not ok():
        pos[a]=slots[sa]
        if b: pos[b]=slots[sb]
        continue
    touched=set(bynet[a])|(set(bynet[b]) if b else set())
    new={n:cost_net(pos,n) for n in touched}; d=sum(new.values())-sum(cn[n] for n in touched)
    if d<=0 or random.random()<math.exp(-d/T):
        slot_of[a]=(g,sb); occ[(g,sb)]=a; occ[(g,sa)]=b
        if b: slot_of[b]=(g,sa)
        cn.update(new)
    else:
        pos[a]=slots[sa]
        if b: pos[b]=slots[sb]
print("HPWL: %.0f mm"%sum(cost_net(pos,n) for n in nets))
for r in range(ROWS):
    print(" ".join(f"{(occ.get(('chip',r*COLS+c)) or '--'):>4}" for c in range(COLS)))
for g in ('back','left','front'):
    print(g, {it:pos[it] for it in groups[g][0]})
json.dump(pos,open(sys.argv[2],'w'))
