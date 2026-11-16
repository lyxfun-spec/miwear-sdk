"""NS6 page component fixture derived from frozen B34 resident model.
Allocator, tick, cache visibility and task context are modeled. No device IO.
"""
from pathlib import Path
import sys,struct,json,hashlib
import argparse
a=argparse.ArgumentParser();a.add_argument('--research',type=Path,required=True);args=a.parse_args()
R=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(args.research/'python-deps'))
sys.path.insert(0,str(args.research/'python-deps'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.arm_const import *
AP=(args.research/'band11/179-vela_ap.bin').read_bytes();CODE=(R/'engineering-build/band_default_bootstrap.bin').read_bytes()
assert hashlib.sha256(AP).hexdigest()=='307adabe0ff61c77be73d045a8260e279cddd131c7a535a2678eb34199278a2b'
M=0x3c410010;ENTRY=0x1c410090;OWNER=0x3c380000;NODE=0x3c378000;OTHER=0x3c378800;LL=0x200bd108
class Machine:
 def __init__(self,mode='normal'):
  self.recent=[];self.mode=mode;self.creator=0x20143810;self.callback_task=0x20150000;self.tick=123;self.started=0;self.returned=0;self.freed=set();self.allocs=0;self.releases=0;self.trace=[]
  self.u=u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  for base in [0x0c0c0000,0x2c0c0000]:u.mem_map(base,(len(AP)+4095)&~4095);u.mem_write(base,AP)
  for base,n in [(0x20000000,0x160000),(0x3c350000,0x80000),(0x1c350000,0x80000),(0x3c410000,0x8000),(0x1c410000,0x8000)]:u.mem_map(base,n)
  raw=CODE[:4]+struct.pack('<I',M)+CODE[8:];u.mem_write(ENTRY,raw);u.mem_write(ENTRY+0x20000000,raw)
  self.write(0x200b2330,0x3c356b40);self.write(0x3c356b5c,0x3c356ca8);self.write(0x3c356b60,0x3cfefdf8)
  self.write(0x200b0070,0x2010d9e0);self.write(0x2010d9fc,0x2010db48);self.write(0x2010da00,0x2015f2b0)
  self.write(0x200b0364,self.creator);self.write(self.creator+44,400);self.write(self.callback_task+44,48)
  self.write(LL,24);self.write(LL+4,OTHER);self.write(LL+8,OTHER);self.write(OTHER+24,0);self.write(OTHER+28,0)
  u.mem_write(OTHER+20,b'\x01');u.mem_write(0x200bd114,b'\x01');self.write(M,0x7ffffffe)
  for a in [0xc3514ec,0xc34cc7c,0xc3a4318,0xc3aa344,0xc92e6da,0xc91ca02,0xc0c181e]:u.hook_add(UC_HOOK_CODE,self.stub,begin=a,end=a)
  u.hook_add(UC_HOOK_CODE,self.hook)
  u.hook_add(UC_HOOK_MEM_READ,self.guard)
 def write(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def word(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def guard(self,u,access,a,size,v,_):assert not any(p<=a<p+n for p,n in self.freed),f'read after free {a:x}'
 def hook(self,u,a,size,_):
  self.recent.append((hex(a),hex(u.reg_read(UC_ARM_REG_XPSR))));self.recent=self.recent[-15:]
  assert not ((OWNER,32768) in self.freed and OWNER-0x20000000<=a<OWNER-0x20000000+32768),'execute after owner free'
  if a==OWNER-0x20000000+4096:
   self.started+=1;assert u.reg_read(UC_ARM_REG_R0)==NODE
   self.trace.append(f'callback{self.started}_entry')
  if a==0xc3ac586:
   self.returned+=1;self.trace.append(f'callback{self.returned}_returned')
 def stub(self,u,a,size,_):
  if a==0xc3514ec:
   assert u.reg_read(UC_ARM_REG_R0)==32768;self.allocs+=1;u.reg_write(UC_ARM_REG_R0,0 if self.mode=='owner_alloc_fail' else OWNER)
  elif a==0xc3a4318:
   assert u.reg_read(UC_ARM_REG_R0)==LL
   p=0 if self.mode=='timer_alloc_fail' else NODE
   if p:
    old=self.word(LL+4);self.write(p+24,0);self.write(p+28,old);self.write(old+24,p);self.write(LL+4,p)
   u.reg_write(UC_ARM_REG_R0,p)
  elif a==0xc34cc7c:
   p=u.reg_read(UC_ARM_REG_R0)
   if p==NODE:
    assert 1<=self.started<=120 and self.returned==self.started-1 and (p,32)not in self.freed
    self.trace.append('timer_free_inside_callback3');self.freed.add((p,32))
   elif p==OWNER:
    assert (1<=self.returned<=120 and (NODE,32)in self.freed)or self.mode=='timer_alloc_fail'
    assert self.releases==0
    self.releases+=1;self.trace.append('owner_free_after_callback_return');self.freed.add((p,32768))
   else:raise AssertionError('foreign free')
  elif a==0xc3aa344:u.reg_write(UC_ARM_REG_R0,self.tick)
  elif a==0xc0c181e:u.mem_write(OWNER-0x20000000,bytes(u.mem_read(OWNER,32768)));u.reg_write(UC_ARM_REG_R0,0)
  else:u.reg_write(UC_ARM_REG_R0,0)
  u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def call(self,entry,arg=0):
  u=self.u;u.reg_write(UC_ARM_REG_XPSR,u.reg_read(UC_ARM_REG_XPSR)&~0x0600fc00);u.reg_write(UC_ARM_REG_R0,arg);u.reg_write(UC_ARM_REG_SP,0x2001fff0);u.reg_write(UC_ARM_REG_LR,0x20010001)
  regs=[UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]
  for r in regs:u.reg_write(r,0x55aa7733)
  try:u.emu_start(entry|1,0x20010000,count=3000000)
  except Exception:
   print(self.recent);print("pc",hex(u.reg_read(UC_ARM_REG_PC)),"lr",hex(u.reg_read(UC_ARM_REG_LR)),"bytes",bytes(u.mem_read(u.reg_read(UC_ARM_REG_PC),8)).hex(),"xpsr",hex(u.reg_read(UC_ARM_REG_XPSR)));raise
  assert u.reg_read(UC_ARM_REG_PC)==0x20010000 and u.reg_read(UC_ARM_REG_SP)==0x2001fff0
  assert all(u.reg_read(r)==0x55aa7733 for r in regs)
 def probe(self,op):self.write(M+4,op);self.call(ENTRY,M);return [self.word(M+i*4)for i in range(32)]
 def dispatch(self):
  task=self.creator if self.mode=='same_task'else self.callback_task
  if self.mode=='bad_callback':task=0x20030000
  self.write(0x200b0364,task)
  self.tick+=1600;self.write(0x200bd128,self.tick);self.call(0xc3ac480)
 def finish(self):
  for _ in range(3):self.dispatch()
from unicorn import UC_HOOK_MEM_WRITE

MAN=0x20084e58;MAPS=[0x3c364000,0x3c364100,0x3c364200]
WATCH=0x3c368000;SECOND=WATCH+128;RECOVER=WATCH+256;TABLE=0x20080900
class IndexMachine(Machine):
 def __init__(self):
  super().__init__();self.write(M+84,48);self.queries=0;self.nodes=[];self.keys=[];self.protected=[]
  self.write(MAN+28,0x2ca3b5cc);self.write(MAN+8,2|(1<<16))
  for slot,p in zip([MAN+20,MAN+24,0x200cac38],MAPS):self.write(slot,p)
  self.make_map(MAPS[0],16,[(b'watch',WATCH,0x10),(b'demo',SECOND,0x20)])
  self.make_map(MAPS[1],8,[(b'restored',RECOVER,0x24)])
  self.make_map(MAPS[2],8,[(b'demo',0xdeadcafe,None),(b'cached.app',0x40000000,None)])
  self.protected.extend([(MAN,32),(0x200cac38,4)])
  self.u.hook_add(UC_HOOK_MEM_WRITE,self.readonly)
  self.u.hook_add(UC_HOOK_CODE,self.audit)
  self.u.hook_add(UC_HOOK_CODE,self.bootrom_compare,begin=0xc7210a8,end=0xc7210a8)
 def bootrom_compare(self,u,a,size,data):
  x=u.reg_read(UC_ARM_REG_R0);y=u.reg_read(UC_ARM_REG_R1)
  for i in range(128):
   left=u.mem_read(x+i,1)[0];right=u.mem_read(y+i,1)[0]
   if left!=right or not left:break
  else:raise AssertionError('unbounded BootROM strcmp')
  u.reg_write(UC_ARM_REG_R0,(left-right)&0xffffffff);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def make_map(self,mp,buckets,items):
  self.write(mp,buckets);self.protected.append((mp,(buckets+1)*4))
  for text,value,ident in items:
   ix=len(self.nodes);node=0x3c36c000+ix*32;key=0x3c370000+ix*256
   name=key+128;h=5381
   for c in text:h=(h*33+c)&0xffffffff
   self.u.mem_write(key,text+b'\0');bucket=mp+4+(h&(buckets-1))*4
   for off,val in [(0,h),(4,key),(8,len(text)+1),(12,value),(16,self.word(bucket))]:self.write(node+off,val)
   self.write(bucket,node)
   if ident is not None:
    self.u.mem_write(name,text+b'\0');self.write(value+12,name);self.write(value+20,ident)
    self.protected.extend([(value,80),(name,len(text)+1)])
   self.nodes.append(node);self.keys.append(key);self.protected.extend([(node,20),(key,len(text)+1)])
 def readonly(self,u,access,a,size,v,data):
  assert not any(p<=a<p+n for p,n in self.protected),f'existing index/app/key written {a:x}'
 def audit(self,u,a,size,data):
  assert a not in [0xc6a9798,0xc6a55b4,0xc6d16a0,0xc6d10d0,0xc8ea580,0xc8ea4e0,0xc6d0b50,0xc546ca0,0xc5468c0],'system mutation'
  if a==0xc6aab08:self.queries+=1
HEAD=0x20084b84;RHDR=0x20084b34
APP=0x3c372000;NAME=0x3c372100;NEWNODE=0x3c372200;NEWKEY=0x3c372300
SYMS=json.loads((R/'engineering-build/build.json').read_text())['targets']['band11']['symbols'];SYMS={k:v&~1 for k,v in SYMS.items() if k in ['owned_callback','app_signal','page_signal','page_create','page_edge','page_destroy','page_resume','page_pause','title_provider']}
class RegistryMachine(IndexMachine):
 def __init__(self,fail=None):
  self.fail=fail;self.requests=[];self.frees=[];self.name_allocs=0;self.unregs=0;self.signal_calls=0;self.id_queries=0;self.name_queries=0;self.mutations=[];self.reorder_on_key=False
  super().__init__()
  self.write(MAN,HEAD);self.write(MAN+4,RHDR)
  self.write(HEAD,SECOND);self.write(HEAD+4,WATCH)
  for a,prev,nxt in [(WATCH,HEAD,SECOND),(SECOND,WATCH,HEAD),(RECOVER,RHDR,RHDR)]:self.write(a,prev);self.write(a+4,nxt)
  self.write(RHDR,RECOVER);self.write(RHDR+4,RECOVER);self.write(MAN+12,2|(3<<16));self.write(MAN+16,1)
 def stub(self,u,a,size,data):
  if a==0xc3514ec and u.reg_read(UC_ARM_REG_R0)in [80,17,20]:
   n=u.reg_read(UC_ARM_REG_R0)
   if n==80:kind='descriptor';p=APP
   elif n==20:kind='node';p=NEWNODE
   else:
    self.name_allocs+=1;kind='name'if self.name_allocs==1 else'key';p=NAME if self.name_allocs==1 else NEWKEY
   self.requests.append(kind)
   if kind=='key'and self.reorder_on_key:
    self.write(HEAD,WATCH);self.write(HEAD+4,SECOND);self.write(SECOND,HEAD);self.write(SECOND+4,WATCH);self.write(WATCH,SECOND);self.write(WATCH+4,HEAD)
   u.reg_write(UC_ARM_REG_R0,0 if self.fail==kind else p);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
  if a==0xc34cc7c:
   p=u.reg_read(UC_ARM_REG_R0)
   if p in [APP,NAME,NEWNODE,NEWKEY]:
    n={APP:80,NAME:17,NEWNODE:20,NEWKEY:17}[p]
    assert(p,n)not in self.freed,'double free'
    if p==APP and self.unregs:assert self.signal_calls==1 and (NEWKEY,17)in self.freed and (NEWNODE,20)in self.freed
    if p==NAME and self.unregs:assert(APP,80)in self.freed
    self.freed.add((p,n));self.frees.append(p)
    u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
   if p==OWNER:assert self.word(OWNER+136)==self.word(OWNER+140)==self.word(OWNER+132)==0 and self.word(OWNER+128)==1
  super().stub(u,a,size,data)
 def readonly(self,u,access,a,size,v,data):
  allowed=(a==MAN+8 and size==2)or(HEAD<=a<HEAD+8)or a in [WATCH,SECOND]or(MAPS[0]+4<=a<MAPS[0]+68)or a in [0x3c36c000+16,0x3c36c020+16]
  if a in [MAN+12,MAN+14]and size==2:allowed=int.from_bytes(u.mem_read(a,2),'little')==v
  if not allowed:assert not any(p<=a<p+n for p,n in self.protected),f'foreign system write{a:x} size{size} value{v:x}'
 def audit(self,u,a,size,data):
  assert a not in [0xc6a9798,0xc8ea580,0xc8ea4e0,0xc6d0b50,0xc546ca0,0xc5468c0,0xc8ebba8,0xc694c44,0xc5dbe84,0xc6d9c3c,0xc6a562a],'builtin install/event/recovery/metadata/launcher/page path called'
  if a==0xc6aa994:self.id_queries+=1
  if a==0xc6aab08:self.name_queries+=1
  if a in [0xc6d16a0,0xc6d10d0]:
   assert u.reg_read(UC_ARM_REG_R0)==MAPS[0] and u.reg_read(UC_ARM_REG_R1)==NAME
   if a==0xc6d16a0:assert u.reg_read(UC_ARM_REG_R2)==APP
   self.mutations.append(a)
  if a==0xc6a55b4:
   assert u.reg_read(UC_ARM_REG_R0)==APP;self.unregs+=1
  if a==OWNER-0x20000000+4096+SYMS['app_signal']:
   assert [u.reg_read(r)for r in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2]]==[APP,5,0]
   assert self.word(HEAD+4)!=APP and self.word(MAN+8)&65535==(self.word(OWNER+160)&65535)
   self.signal_calls+=1
PAGE=OWNER+256;PAGEID=0x7b1c0001;ROOT=0x20084dc4;OLDPAGE=0x20085000
PTABLE=0x3c374300;PMAP=0x3c374000;PNODE=0x3c374100;PKEY=0x3c374200
class PageMachine(RegistryMachine):
 def __init__(self,fail=None):
  self.page_nodes=0;self.page_mutations=[];self.page_returns=[];self.page_frees=[]
  super().__init__(fail)
  self.write(0x20084e3c,ROOT);self.write(0x20084e48,0xc694c45);self.write(0x20084e4c,0xc695221)
  self.write(ROOT+68,OLDPAGE);self.write(OLDPAGE+64,ROOT);self.write(OLDPAGE+20,0x100000);self.write(OLDPAGE+16,0x2cb40eef)
  self.write(0x200c2d44,0x10001);self.write(0x200c2d48,48);self.write(0x200c2d4c,0x3c377000);self.write(0x200c2d50,0x3c377000)
  self.write(0x200c2d38,48)
  self.u.hook_add(UC_HOOK_CODE,self.calloc_map,begin=0xc3511b0,end=0xc3511b0)
 def calloc_map(self,u,a,size,data):
  assert u.reg_read(UC_ARM_REG_R0)==68
  self.requests.append('page_map')
  if self.fail!='page_map':u.mem_write(PMAP,bytes(68))
  u.reg_write(UC_ARM_REG_R0,0 if self.fail=='page_map'else PMAP);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def stub(self,u,a,size,data):
  if a==0xc3514ec:
   n=u.reg_read(UC_ARM_REG_R0)
   kind=None;p=0
   if n==12:kind='page_table';p=PTABLE
   elif n==7:kind='page_key';p=PKEY
   elif n==20:
    self.page_nodes+=1
    if self.page_nodes==1:kind='page_node';p=PNODE
   if kind:
    self.requests.append(kind);u.reg_write(UC_ARM_REG_R0,0 if self.fail==kind else p);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
  if a==0xc34cc7c and u.reg_read(UC_ARM_REG_R0)in [PTABLE,PMAP,PNODE,PKEY]:
   p=u.reg_read(UC_ARM_REG_R0);n={PTABLE:12,PMAP:68,PNODE:20,PKEY:7}[p]
   assert(p,n)not in self.freed,'page private double free'
   self.page_frees.append(p);self.frees.append(p);self.freed.add((p,n))
   u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
  if a==0xc34cc7c and u.reg_read(UC_ARM_REG_R0)==APP and self.unregs:
   assert self.page_frees==[PKEY,PNODE,PMAP,PTABLE]
  super().stub(u,a,size,data)
 def readonly(self,u,access,a,size,v,data):
  if a in [ROOT+68,OLDPAGE+64]or(a==MAN+12 and size==2):return
  assert not (0x200c2d34<=a<0x200c2d54),'navigation headers modified'
  super().readonly(u,access,a,size,v,data)
 def audit(self,u,a,size,data):
  if a in [0xc6d16a0,0xc6d10d0]and u.reg_read(UC_ARM_REG_R0)==PMAP:
   assert u.reg_read(UC_ARM_REG_R1)==0x2cb40eef
   if a==0xc6d16a0:assert u.reg_read(UC_ARM_REG_R2)==PAGE
   self.page_mutations.append(a);return
  if a==0xc8e858c:assert u.reg_read(UC_ARM_REG_R0)==PMAP
  if a in [0xc694c44,0xc695220]:
   assert u.reg_read(UC_ARM_REG_R0)==PAGE;self.page_returns.append(a)
  if a==OWNER-0x20000000+4096+SYMS['page_signal']:
   assert u.reg_read(UC_ARM_REG_R0)==PAGE and u.reg_read(UC_ARM_REG_R1)in [1,2]and not u.reg_read(UC_ARM_REG_R2)
  # Base bans raw register only for its metadata-only scope; B23 allows PAGE.
  if a==0xc694c44:return
  super().audit(u,a,size,data)
REGROOT=ROOT;IDENT=PAGEID
PARENT=0x3c3a0000;P_SPEC=0x3c3a0100;P_ARRAY=0x3c3a0200;ROOTOBJ=0x3c3a0400
R_SPEC=0x3c3a0600;R_ARRAY=0x3c3a0700;LABEL=0x3c3a0800;TEXT=0x3c3a0900;DEFAULT=0x3c3a0940;GESTURE=0x3c3a1000
G_SPEC=0x3c3a1100;G_ARRAY=0x3c3a1200
# symbols loaded above
class RootMachine(PageMachine):
 def __init__(self,mode='normal'):
  super().__init__(mode);self.signals=[];self.entries=[];self.ui_calls=[];self.parent_children=[GESTURE];self.root_children=[];self.gesture_children=[]
  self.write(M+84,48);self.write(0x20084e3c,REGROOT);self.write(REGROOT+68,OLDPAGE)
  self.write(OLDPAGE+20,0x100000);self.write(OLDPAGE+64,REGROOT);self.write(OLDPAGE+68,0)
  self.write(0x200c2d54,PARENT);self.write(PARENT,0x2ca3b9f4);self.write(PARENT+8,P_SPEC)
  self.write(GESTURE,0x2ca3b9f4);self.write(GESTURE+8,G_SPEC);self.write(0x200d20b8,GESTURE);self.write(GESTURE+4,PARENT);self.write(0x200d208c,0)
  self.sync()
  for a in [0xc381670,0xc382f40,0xc350364,0xc6d0dd4,0xc3836d2,0xc4f8724,0xc380a06,0xc8b791e,0xc37f8be,0xc384940,0xc380a1c,0xc3821da,0xc381eec,0xc3b3428,0xc3abd88,0xc3abdc0,0xc721270,0xc90e6d8,0xc3813ac,0xc383b90,0xc37fb98]:
   self.u.hook_add(UC_HOOK_CODE,self.stub_ui,begin=a,end=a)
 def sync(self):
  for spec,array,children in [(P_SPEC,P_ARRAY,self.parent_children),(R_SPEC,R_ARRAY,self.root_children),(G_SPEC,G_ARRAY,self.gesture_children)]:
   self.write(spec,array);self.u.mem_write(spec+48,struct.pack('<H',len(children)))
   for i,child in enumerate(children):self.write(array+i*4,child)
 def audit(self,u,a,size,data):
  for name,offset in SYMS.items():
   if name not in ['owned_callback','app_signal','title_provider']and a==OWNER-0x20000000+4096+offset:
    assert u.reg_read(UC_ARM_REG_R0)==PAGE
    self.entries.append(name)
    if name=='page_signal':self.signals.append(u.reg_read(UC_ARM_REG_R1))
    if name=='page_destroy':
     assert self.word(PAGE+48)==ROOTOBJ and (ROOTOBJ,0x114)not in self.freed
     if self.mode=='destroy_wrong_task':self.write(self.callback_task+44,49)
  if a==0xc34cc7c and u.reg_read(UC_ARM_REG_R0)==OWNER and self.mode!='timer_alloc_fail':
   assert self.word(PAGE+48)==0 and ROOTOBJ not in self.parent_children
   assert self.word(REGROOT+68)==OLDPAGE and self.word(OLDPAGE+64)==REGROOT
   assert self.word(0x200d208c)==0
 def stub_ui(self,u,a,size,data):
  r0=u.reg_read(UC_ARM_REG_R0);r1=u.reg_read(UC_ARM_REG_R1)
  # Timer's own free wrapper remains real, forwarding to modeled heap free.
  if a==0xc3abdc0 and r0==NODE:return
  self.ui_calls.append(a)
  assert self.word(0x200b0364)==self.callback_task
  if a==0xc381670:
   if r0==0x2ca3ba18:
    assert r1==PARENT and ROOTOBJ not in self.parent_children
    self.parent_children.append(ROOTOBJ);self.sync();self.write(ROOTOBJ,r0);self.write(ROOTOBJ+4,PARENT);self.write(ROOTOBJ+8,R_SPEC)
    u.reg_write(UC_ARM_REG_R0,ROOTOBJ);self.trace.append('framework_root_created')
   elif r0==0x2ca159b4:
    assert r1==ROOTOBJ;self.root_children.append(LABEL);self.sync()
    self.write(LABEL,r0);self.write(LABEL+4,ROOTOBJ);self.write(LABEL+52,DEFAULT);self.write(LABEL+0x6c,0);self.u.mem_write(DEFAULT,b'Text\0')
    u.reg_write(UC_ARM_REG_R0,LABEL)
   else:raise AssertionError('unexpected class constructor')
  elif a==0xc382f40:assert r0 in [ROOTOBJ,LABEL] and r1==0
  elif a==0xc3836d2:assert r0==ROOTOBJ and r1==212 and u.reg_read(UC_ARM_REG_R2)==520
  elif a in [0xc350364,0xc6d0dd4]:pass
  elif a==0xc4f8724:u.reg_write(UC_ARM_REG_R0,0x3c3a1800)
  elif a==0xc380a06:u.reg_write(UC_ARM_REG_R0,0)
  elif a==0xc8b791e:assert r0==PARENT
  elif a==0xc37f8be:assert r0==GESTURE
  elif a==0xc384940:
   assert r0==ROOTOBJ and r1==GESTURE
   self.parent_children.remove(ROOTOBJ);self.gesture_children.append(ROOTOBJ);self.write(ROOTOBJ+4,GESTURE);self.sync()
   if self.mode=='framework_clears_hidden':self.write(ROOTOBJ+44,self.word(ROOTOBJ+44)&~1)
   self.trace.append('set_parent_moves_root_A_to_B')
  elif a==0xc380a1c:u.reg_write(UC_ARM_REG_R0,int(not(self.word(r0+44)&1)))
  elif a in [0xc3821da,0xc381eec,0xc383b90]:assert r0 in [ROOTOBJ,LABEL,PARENT,GESTURE]
  elif a==0xc721270:
   assert r0==OWNER+832;value=bytes(u.mem_read(r0,64)).split(bytes(1),1)[0]
   assert value in [b'NATIVE B34\nRADIO WAIT\nUse BACK',b'NATIVE B34\nRADIO READY\nUse BACK'];u.reg_write(UC_ARM_REG_R0,len(value))
  elif a==0xc3abd88:assert 0<r0<=32;u.reg_write(UC_ARM_REG_R0,TEXT)
  elif a==0xc3abdc0:assert r0 in [DEFAULT,TEXT];self.freed.add((r0,16 if r0==DEFAULT else 31))
  elif a==0xc90e6d8:assert r0==TEXT and r1==OWNER+832;u.mem_write(r0,bytes(u.mem_read(r1,32)))
  elif a==0xc3b3428:assert r0==LABEL
  elif a==0xc3813ac:
   assert r0==ROOTOBJ and ROOTOBJ in self.gesture_children
   if self.mode=='destroy_wrong_task':self.write(self.callback_task+44,48)
   assert self.word(OWNER+564)==self.word(OWNER+560) and self.word(OWNER+600)==1,'destroy callback before framework root deletion'
   self.gesture_children.remove(ROOTOBJ);self.root_children=[];self.sync()
   self.freed.update([(ROOTOBJ,0x114),(LABEL,112),(TEXT,31)])
   self.trace.append('framework_deletes_root_after_destroy_callback')
  elif a==0xc37fb98:assert r0 in [PARENT,GESTURE]
  else:raise AssertionError('unknown UI dependency')
  u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
NAVHEAD=0x200c2d44;GROUP=0x3c3b0000;BASEENTRY=0x3c3b0100;WATCHAPP=WATCH;OLDROOT=0x3c3a3000
class NavMachine(RootMachine):
 def __init__(self,mode='normal'):
  super().__init__(mode);self.pool=0x3c3b1000;self.nav_allocs={};self.pending=[];self.nav_calls=[];self.event44=0;self.filter_state=1
  self.write(OLDPAGE+20,0x00100000);self.write(OLDPAGE+24,6);self.write(OLDPAGE+40,0x00020511);self.write(OLDPAGE+48,OLDROOT)
  self.write(NAVHEAD,0x10001);self.write(NAVHEAD+4,48);self.write(NAVHEAD+8,GROUP);self.write(NAVHEAD+12,GROUP)
  self.write(0x200c2d38,48);self.write(0x200c2d34,0);self.write(0x200c2d3c,0);self.write(0x200c2d40,0)
  self.write(GROUP,WATCHAPP);self.write(WATCHAPP+20,0x10);self.write(GROUP+4,1);self.write(GROUP+8,NAVHEAD);self.write(GROUP+12,4);self.write(GROUP+16,BASEENTRY);self.write(GROUP+20,BASEENTRY)
  self.write(BASEENTRY,OLDPAGE);self.write(OLDROOT,0x2ca3ba18);self.write(OLDROOT+4,GESTURE);self.write(OLDROOT+8,0x3c3a3400)
  self.gesture_children.append(OLDROOT);self.sync()
  self.write(0x200c2d28,8);self.write(0x200c2d2c,0);self.write(0x200c2d30,0)
  self.u.mem_write(0x200c23c0,b'\x02');self.u.mem_write(0x20095f2d,b'\x01')
  for a in [0xc3a460a,0xc3abdc0,0xc720f08,0xc691a7c,0xc383a78,0xc6b7490,0xc502b38,0xc88fc6c,0xc383f3c]:self.u.hook_add(UC_HOOK_CODE,self.nav_stub,begin=a,end=a)
  self.u.hook_add(UC_HOOK_CODE,self.nav_audit)
 def nav_audit(self,u,a,size,data):
  if a in [0xc8ebba8,0xc6999f4,0xc695060,0xc691e28,0xc69553c,0xc505fb0,0xc69987c,0xc691f10]:self.nav_calls.append(a)
 def audit(self,u,a,size,data):
  super().audit(u,a,size,data)
  if a==0xc34cc7c and u.reg_read(UC_ARM_REG_R0)==OWNER:
   assert self.word(NAVHEAD)==0x10001 and self.word(GROUP+16)==self.word(GROUP+20)==BASEENTRY
   assert self.word(OLDPAGE+36)==0 and self.word(OLDPAGE+40)&255==17 and not self.pending
   assert self.word(0x200c2d2c)==0 and self.word(0x200d2061)&255==0
 def stub_ui(self,u,a,size,data):
  r0=u.reg_read(UC_ARM_REG_R0)
  if a==0xc3abdc0 and r0 in self.nav_allocs:return # handled by nav_stub
  if a in [0xc3821da,0xc381eec,0xc383b90,0xc37fb98,0xc380a1c]and r0==OLDROOT:
   if a==0xc380a1c:u.reg_write(UC_ARM_REG_R0,int(not(self.word(r0+44)&1)))
   u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
  if a==0xc3813ac:assert ROOTOBJ in self.gesture_children
  super().stub_ui(u,a,size,data)
 def nav_stub(self,u,a,size,data):
  r0=u.reg_read(UC_ARM_REG_R0);r1=u.reg_read(UC_ARM_REG_R1);r2=u.reg_read(UC_ARM_REG_R2)
  if a==0xc3abdc0:
   if r0 not in self.nav_allocs:return
   assert not any(p==r0 for p,n in self.freed);self.freed.add((r0,self.nav_allocs[r0]))
  elif a==0xc3a460a:
   stride=self.word(r0);assert stride in [4,8,48]
   node=self.pool;self.pool+=0x80;self.nav_allocs[node]=stride+8
   tail=self.word(r0+8);self.write(node+stride,tail);self.write(node+stride+4,0)
   if tail:self.write(tail+stride+4,node)
   else:self.write(r0+4,node)
   self.write(r0+8,node);u.reg_write(UC_ARM_REG_R0,node)
  elif a==0xc720f08:u.mem_write(r0,bytes([r1&255])*r2)
  elif a==0xc691f80:
   assert r1==GROUP
   if self.word(NAVHEAD)>>16==2:assert self.word(PAGE+16)==0x2cb40eef # no transient owned-name pointer escapes via group observer payload
   self.event44+=1 # synchronous notification of current page-name list
  elif a==0xc691a7c:pass # app history metadata notification
  elif a==0xc383a78:assert r0==ROOTOBJ
  elif a==0xc383f3c:assert r0==LABEL and r1==ROOTOBJ and r2==9
  elif a==0xc6b7490:
   assert r0 in [0,1];self.filter_state=r0 # actual helper argument observed, input callback lists modeled
  elif a==0xc8ea580:assert r0 in [0x5c,0x2c] # synchronous event notify modeled
  elif a==0xc502b38:
   # Actual transition planner provides page/root/callback/NULL config.
   page=self.word(r1);root=self.word(r1+4);cb=self.word(r1+8)
   assert (page,root)in [(PAGE,ROOTOBJ),(OLDPAGE,OLDROOT)]
   assert cb in [0xc695455,0xc6955f9] and self.word(r1+12)==0
   self.pending.append((root,cb))
  elif a==0xc88fc6c:
   assert r0 in [ROOTOBJ,OLDROOT] and r1 in [0xc695455,0xc6955f9]
   self.pending.append((r0,r1))
  else:raise AssertionError(hex(a))
  u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def complete(self):
  self.write(0x200b0364,self.callback_task)
  # Old-out must complete before new-in drains retired pages.
  for root,cb in sorted(self.pending,key=lambda x:x[1]):
   self.write(0x3c3bf000,root);self.call(cb&~1,0x3c3bf000)
  self.pending.clear()
 def run(self):
  self.probe(0);self.dispatch();self.complete()
  self.dispatch();self.dispatch();self.dispatch();return self.probe(1)



class OwnMachine(NavMachine):
 def __init__(self,fail=None):
  super().__init__(fail or 'normal');self.generic={};self.generic_pool=0x3c3c0000;self.usage=[];self.notifications=[]
  self.write(0x20096cec,0x20096cec);self.write(0x20096cf0,0x20096cec);self.write(0x200f19e0,1)
  self.write(0x20096cf8,0x20096cf8);self.write(0x20096cfc,0x20096cf8);self.write(0x200c1b98,1)
  for a in [0xc69e5a4,0xc34d536,0xc537ebc,0xc6d7dac,0xc35118c,0xc34921e,0xc3f5668]:self.u.hook_add(UC_HOOK_CODE,self.extra,begin=a,end=a)
 def extra(self,u,a,size,data):
  r0=u.reg_read(UC_ARM_REG_R0);r1=u.reg_read(UC_ARM_REG_R1);r2=u.reg_read(UC_ARM_REG_R2)
  if a==0xc3f5668:pass
  elif a in [0xc69f91c,0xc69e5a4]:u.reg_write(UC_ARM_REG_R0,0)
  elif a in [0xc34d536,0xc537ebc]:u.reg_write(UC_ARM_REG_R0,self.tick);u.reg_write(UC_ARM_REG_R1,0)
  elif a==0xc6d7dac:
   record=r0;assert self.word(record)==2
   name=bytes(u.mem_read(record+8,64)).split(b'\0')[0];assert name==b'b11.native.page6';assert record+8!=NAME
   self.usage.append(name)
  elif a==0xc34921e:
   # strncpy leaf; source bytes copied, no original pointer retained.
   value=bytes(u.mem_read(r1,r2)).split(b'\0')[0];u.mem_write(r0,(value+bytes(r2))[:r2])
  elif a==0xc35118c:
   n=r0*r1;p=self.generic_pool;self.generic_pool+=0x100;self.generic[p]=max(n,4);u.mem_write(p,bytes(max(n,4)));u.reg_write(UC_ARM_REG_R0,p)
  u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def stub(self,u,a,size,data):
  r0=u.reg_read(UC_ARM_REG_R0)
  if a==0xc3514ec and (r0 not in [32768,80,17,20,12,7] or (r0==12 and "page_table"in self.requests)):
   p=self.generic_pool;self.generic_pool+=0x100;self.generic[p]=r0;u.reg_write(UC_ARM_REG_R0,p);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
  if a==0xc34cc7c and r0 in self.generic:
   assert not any(p==r0 for p,n in self.freed);self.freed.add((r0,self.generic[r0]));u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
  PageMachine.stub(self,u,a,size,data)
 def readonly(self,u,access,a,size,v,data):
  if (0x200c2d28<=a<0x200c2d54)or a in [ROOT+68,OLDPAGE+64]or(a==MAN+12 and size==2):return
  RegistryMachine.readonly(self,u,access,a,size,v,data)
 def audit(self,u,a,size,data):
  RootMachine.audit(self,u,a,size,data)
  assert a not in [0xc6a9798,0xc546ca0,0xc5468c0,0xc5dbe84,0xc6d9c3c,0xc6a562a], 'install/metadata/launcher path'
  if a in [0xc6d16a0,0xc6d10d0]:
   mp=u.reg_read(UC_ARM_REG_R0);assert mp in [MAPS[0],PMAP]
   if mp==PMAP:self.page_mutations.append(a)
  if a in [0xc694c44,0xc695220]:assert u.reg_read(UC_ARM_REG_R0)==PAGE;self.page_returns.append(a)
  if a==0xc6a55b4:assert u.reg_read(UC_ARM_REG_R0)==APP;self.unregs+=1
  if a==OWNER-0x20000000+4096+SYMS['app_signal']:
   assert u.reg_read(UC_ARM_REG_R0)==APP and u.reg_read(UC_ARM_REG_R1)<256
   if u.reg_read(UC_ARM_REG_R1)==5:self.signal_calls+=1
  if a==0xc8ea580:
   assert u.reg_read(UC_ARM_REG_R0)in [0x2c,0x5c]
   if u.reg_read(UC_ARM_REG_R0)==0x5c:assert u.reg_read(UC_ARM_REG_R1)in [0,1]and u.reg_read(UC_ARM_REG_R2)==0
   if u.reg_read(UC_ARM_REG_R0)==0x2c:
    r=u.reg_read(UC_ARM_REG_R1);assert self.word(r+4)==NAME;assert u.reg_read(UC_ARM_REG_R2)==1
    self.notifications.append(self.word(r)&65535)
 def complete(self):
  super().complete();self.call(0xc6d0b50)
 def run(self):
  self.probe(0)
  for _ in range(10):
   self.dispatch();self.complete()
   if (NODE,32)in self.freed:break
  return self.probe(1)

PBASE=0x3c3c8000;SBASE=0x3c3c9000
class LauncherMachine(OwnMachine):
 def __init__(self,fail=None,empty=False):
  super().__init__(fail)
  self.empty=empty;self.launch_nodes={};self.launch_frees=[];self.protected_launch=[]
  for base,source,n in [(0x20087298,0x2cc5276c,8),(0x200872d0,0x2cc527a4,2)]:self.u.mem_write(base,bytes(self.u.mem_read(source,n*4)))
  for i,v in enumerate([0xc5415dd,0xc5415cd,0xc548329,0xc5489a9]):self.write(0x200c5d78+i*4,v)
  self.u.mem_write(0x20087248,b'\4');self.write(0x20087250,0);self.u.mem_write(0x200c5b3e,b'\3\1')
  for k in range(2):
   stride=129 if k else 28;h=0x20087334+k*12;nodes=[]if empty else[(SBASE if k else PBASE)+i*0x100 for i in range(2)]
   self.write(h,stride);self.write(h+4,nodes[0]if nodes else 0);self.write(h+8,nodes[-1]if nodes else 0)
   for i,a in enumerate(nodes):
    text=[b'demo',b'settings'][i]+b'\0'
    if k:self.u.mem_write(a,text+bytes(128-len(text)));self.u.mem_write(a+128,b'\x0e')
    else:self.write(a,0x20+i);self.write(a+4,0x2cb19092);self.write(a+8,SBASE+i*0x100);self.write(a+12,SBASE+i*0x100)
    self.write(a+stride,nodes[i-1]if i else 0);self.write(a+stride+4,nodes[i+1]if i+1<len(nodes)else 0)
    self.protected_launch.append((a,stride))
  self.u.hook_add(UC_HOOK_MEM_WRITE,self.readonly_launch)
 def readonly_launch(self,u,access,a,size,v,data):
  assert not any(p<=a<p+n for p,n in self.protected_launch),'foreign launcher data modified'
  assert not any(p<=a<p+n for p,n in [(0x20087298,32),(0x200872d0,8),(0x200c5d78,16),(0x20087220,108),(0x200c5b3e,2)]),'launcher UI/method/style state modified'
 def stub(self,u,a,size,data):
  r0=u.reg_read(UC_ARM_REG_R0)
  if a==0xc3514ec and r0 in [36,137]:
   kind='launcher_primary'if r0==36 else'launcher_secondary';self.requests.append(kind)
   p=self.generic_pool;self.generic_pool+=0x100
   if self.fail==kind:p=0
   else:self.generic[p]=r0;self.launch_nodes[p]=r0
   u.reg_write(UC_ARM_REG_R0,p);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
  if a==0xc34cc7c and r0 in self.launch_nodes:
   assert (self.word(OWNER+728)&3)in ([0,1]if self.launch_nodes[r0]==137 else[0]),'node free only after actual unlink'
   self.launch_frees.append(r0)
  super().stub(u,a,size,data)
 def run(self):
  self.probe(0)
  for _ in range(10):
   self.dispatch();self.complete()
   if(NODE,32)in self.freed:break
  return self.probe(1)


class ResidentMachine(LauncherMachine):
 def stub_ui(self,u,a,size,data):
  r0=u.reg_read(UC_ARM_REG_R0)
  if a==0xc381670:
   if r0==0x2ca3ba18:self.freed.discard((ROOTOBJ,0x114))
   if r0==0x2ca159b4:self.freed.discard((LABEL,112));self.freed.discard((DEFAULT,16))
  if a==0xc3abd88:self.freed.discard((TEXT,31))
  super().stub_ui(u,a,size,data)
 def open(self):
  for reg in [UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]:self.u.reg_write(reg,0)
  self.call(0xc8ebba8,PAGEID);self.complete();self.dispatch()
 def back(self):
  self.call(0xc6999f4,PAGEID);self.complete();self.dispatch()


class PageOnly(ResidentMachine):
 def __init__(self,fail=None):
  super().__init__(fail)
  self.write(self.callback_task+8,0x20151000)
  self.write(0x20151000,42)
  # BLE bridge independently exercised by verify_page.py; this suite executes
  # actual registry/navigation/UI ROM and callbacks without spawning workers.
  off=json.loads((R/'engineering-build/build.json').read_text())['targets']['band11']['symbols']['ns5page_poll']&~1
  self.u.hook_add(UC_HOOK_CODE,self.skip_poll,begin=OWNER-0x20000000+4096+off,end=OWNER-0x20000000+4096+off)
 def skip_poll(self,u,a,n,data):u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def stub_ui(self,u,a,n,data):
  r0=u.reg_read(UC_ARM_REG_R0);r1=u.reg_read(UC_ARM_REG_R1)
  if a==0xc721270:
   assert r0==OWNER+2048;value=bytes(u.mem_read(r0,128)).split(b'\0')[0];assert value.startswith(b'NS6 NATIVE');u.reg_write(UC_ARM_REG_R0,len(value))
  elif a==0xc3abd88:
   assert 0<r0<=128;self.freed.discard((TEXT,31));u.reg_write(UC_ARM_REG_R0,TEXT)
  elif a==0xc90e6d8:
   assert r0==TEXT and r1==OWNER+2048;u.mem_write(r0,bytes(u.mem_read(r1,128)).split(b"\0")[0]+b"\0")
  else:return super().stub_ui(u,a,n,data)
  u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
results=[]
for fail in [None,'descriptor','name','page_table','page_map','page_key','launcher_primary','launcher_secondary']:
 m=PageOnly(fail);m.call(ENTRY,M);m.dispatch()
 if fail:
  assert m.word(OWNER+76) and not m.word(OWNER+8),fail
 else:
  assert m.word(OWNER+772)==1 and not m.word(OWNER+76),m.word(OWNER+76)
  for _ in range(3):m.open();m.back()
  assert [m.word(OWNER+i) for i in [560,564,568,572,576,580]]==[3]*6
  assert m.word(OWNER+808)==3 and m.word(OWNER+304)==0 and m.word(OWNER+292)==0
  assert m.word(OWNER+812)==3 and m.word(OWNER+816)==3 and not m.word(OWNER+76)
  assert (OWNER,32768) not in m.freed and not m.releases
  # New Lua bootstrap must attach to this timer/immutable runtime, no duplicate.
  count=m.allocs;m.write(M+4,0);m.call(ENTRY,M);assert m.word(M+8)==OWNER and m.word(M+104)==1 and m.allocs==count
 results.append({'case':fail or 'three native ROM page cycles + attach','passed':True})
(R/'engineering-build/band-ui-verification.json').write_text(json.dumps({'hardware_tested':False,'artifacts':{n:hashlib.sha256((R/'engineering-build'/n).read_bytes()).hexdigest() for n in ['band_default.bin','band_default_bootstrap.bin']},'cases':results,'real_code':'NS6 bootstrap/registration/list publication/native callbacks and ROM navigation, page lifecycle, deferred destruction, text copy','models':'widgets/allocator/timer/task/cache and user open/BACK; ns5page_poll intentionally stubbed, BLE covered separately; full application-list renderer not exercised'},indent=2))
print('Band NS6 UI',len(results),'PASS')
