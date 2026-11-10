# Experiment: chips in rows; each row upright (0/180) or lying (90/270), its rotation chosen by the
# annealer. Non-DIP parts stay where the Board has them.
# usage: place3.py list                       -> row patterns that fit
#        place3.py PATTERN FLIPS SEED OUT_JSON   PATTERN like UUUULL, FLIPS 0|1
import re,random,math,sys,json,collections,itertools
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
chips=sorted([r for r in fp if npins(r)],key=lambda r:(r[0],int(r[1:])))
pcb=open('boards/tennis/tennis.kicad_pcb').read()
fixed={}
for m in re.finditer(r'\(footprint "([^"]+)"',pcb):
    seg=pcb[m.start():m.start()+3000]
    at=re.search(r'\(at ([-\d.]+) ([-\d.]+)',seg); ref=re.search(r'\(property "Reference" "([^"]+)"',seg).group(1)
    if not npins(ref): fixed[ref]=(float(at.group(1)),float(at.group(2)),0)
X0,XMAX,Y0,HMAX=45.0,228.62,22.0,135.0
SHAPE={'U':dict(w=7.62,h=17.78,xp=16.0,yp=27.0,rots=(0,180)),
       'L':dict(w=17.78,h=7.62,xp=26.0,yp=17.0,rots=(90,270))}
def capacity(s): return int((XMAX-X0-SHAPE[s]['w'])//SHAPE[s]['xp'])+1
def height(p): return sum(SHAPE[s]['yp'] for s in p[:-1])+SHAPE[p[-1]]['h']
def patterns():
    out=[]
    for R in range(3,9):
        for p in itertools.product('UL',repeat=R):
            p=''.join(p)
            if sum(capacity(s) for s in p)>=len(chips) and height(p)<=HMAX: out.append(p)
    return out
if sys.argv[1]=='list':
    print(' '.join(patterns())); sys.exit()
PAT,FLIPS,SEED,OUT=sys.argv[1],sys.argv[2]=='1',int(sys.argv[3]),sys.argv[4]
rowy=[];y=Y0
for s in PAT: rowy.append(y); y+=SHAPE[s]['yp']
slots=[(r,i) for r,s in enumerate(PAT) for i in range(capacity(s))]
def rot(dx,dy,a):
    c=round(math.cos(math.radians(a)));s=round(math.sin(math.radians(a)))
    return (dx*c+dy*s,-dx*s+dy*c)
def origin(r,i,n,a):
    s=PAT[r]; bx=X0+i*SHAPE[s]['xp']; by=rowy[r]; L=(n//2-1)*2.54
    return {0:(bx,by),180:(bx+7.62,by+L),90:(bx,by+7.62),270:(bx+L,by)}[a]
def local(n,p):
    p=int(p); return (0.0,(p-1)*2.54) if p<=n//2 else (7.62,(n-p)*2.54)
rowrot=[SHAPE[s]['rots'][0] for s in PAT]
place={}
def put(c,slot):
    r,i=slot; n=npins(c); a=rowrot[r]; ox,oy=origin(r,i,n,a); place[c]=(ox,oy,a)
def pin(ref,p):
    if ref in fixed: return fixed[ref][:2]
    ox,oy,a=place[ref]; dx,dy=rot(*local(npins(ref),p),a); return (ox+dx,oy+dy)
def cost_net(n):
    pts=[pin(r,p) for r,p in nets[n]]
    xs=[q[0] for q in pts]; ys=[q[1] for q in pts]
    return max(xs)-min(xs)+max(ys)-min(ys)
bynet=collections.defaultdict(set)
for n,nodes in nets.items():
    for r,_ in nodes: bynet[r].add(n)
random.seed(SEED)
pick=random.sample(range(len(slots)),len(chips))
slot_of={};occ={}
for c,k in zip(chips,pick): slot_of[c]=slots[k]; occ[slots[k]]=c; put(c,slots[k])
cn={n:cost_net(n) for n in nets}
N=600000
for it in range(N):
    T=200.0*(0.0005/200.0)**(it/N)
    if FLIPS and random.random()<0.05:
        r=random.randrange(len(PAT)); old_a=rowrot[r]; rs=SHAPE[PAT[r]]['rots']
        rowrot[r]=rs[1] if old_a==rs[0] else rs[0]
        members=[c for c in chips if slot_of[c][0]==r]
        for c in members: put(c,slot_of[c])
        touched=set().union(*(bynet[c] for c in members)) if members else set()
        new={n:cost_net(n) for n in touched}; d=sum(new.values())-sum(cn[n] for n in touched)
        if d<=0 or random.random()<math.exp(-d/T): cn.update(new)
        else:
            rowrot[r]=old_a
            for c in members: put(c,slot_of[c])
        continue
    a=random.choice(chips); sb=random.choice(slots); sa=slot_of[a]
    if sb==sa: continue
    b=occ.get(sb)
    put(a,sb)
    if b: put(b,sa)
    touched=bynet[a]|(bynet[b] if b else set())
    new={n:cost_net(n) for n in touched}; d=sum(new.values())-sum(cn[n] for n in touched)
    if d<=0 or random.random()<math.exp(-d/T):
        slot_of[a]=sb; occ[sb]=a; occ[sa]=b
        if b: slot_of[b]=sa
        cn.update(new)
    else:
        put(a,sa)
        if b: put(b,sb)
total=sum(cost_net(n) for n in nets)
print(f"{PAT} flips={int(FLIPS)} seed={SEED} HPWL {total:.0f} mm")
for r,s in enumerate(PAT):
    print(f"  {s}{rowrot[r]:>4}: "+" ".join(f"{(occ.get((r,i)) or '--'):>4}" for i in range(capacity(s))))
json.dump(place,open(OUT,'w'))
