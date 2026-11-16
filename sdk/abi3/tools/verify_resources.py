"""Compiled retirement implementation + exact retail server/client lifecycle."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--research',type=Path,required=True);a=p.parse_args();sys.path.insert(0,str(a.research/'python-deps'))
 from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
 from unicorn import arm_const as ar
 from prove_retirement_rom import ServerRom
 ap=(a.research/'band11/179-vela_ap.bin').read_bytes();sy=json.loads((R/'build/model-symbols.json').read_text());raw=(R/'build/model.bin').read_bytes()
 CODE=0x1c500800;CLIENT=0x3c510000;U=0x3c50c000;C=U+128;B=0x3c500100;GUARD=B+128;CB=0x3c50f000;PACK=0x3c512000;SYNC=0x3c50d000;STOP=0x20000000;CALLBACK=STOP+16
 regs=[ar.UC_ARM_REG_R0,ar.UC_ARM_REG_R1,ar.UC_ARM_REG_R2,ar.UC_ARM_REG_R3]
 class Model:
  def __init__(self,mode):
   self.mode=mode;self.server=ServerRom(a.research,0x3c560000);self.u=u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
   for p,n in [(0xc0c0000,0xc00000),(0x2c0c0000,0xc00000),(0x1c500000,0x20000),(0x3c500000,0x100000),(STOP,0x1000)]:u.mem_map(p,n)
   u.mem_write(0xc0c0000,ap);u.mem_write(0x2c0c0000,ap);u.mem_write(CODE,raw)
   names='size token node remote client callbacks head state terminal ever_attempted transport_status service_status guard_fence barrier_ok'.split()
   self.layout=dict(zip(names,struct.unpack('<14I',u.mem_read(CODE+sy['resource_layout'],56))))
   self.heap=0x3c530000;self.allocations={};self.freed=[];self.calls=[];self.owner=True;self.live=True;self.callback_count=0;self.delivery_code=0;self.end=STOP;self.deletes=0
   self.put(C,3,CLIENT,4,5,6);self.put(U,1,2,48,0,0);self.put(CLIENT+76,CLIENT+76,CLIENT+76);self.put(CB,52);self.put(CB+8,CALLBACK|1);self.put(CB+28,CALLBACK|1)
   stubs={'nsp_valid':lambda:int(0x3c500000<=self.reg(0)<0x3c600000 and self.reg(1)<=0x3c600000-self.reg(0)),'nsp_ui_matches':lambda:int(self.owner),'nsr_live':lambda:int(self.live),'nsp_alloc':lambda:self.alloc(self.reg(0)),'nsp_free':self.free,'nsr_rpc':self.rpc,'nsr_barrier':lambda:0 if self.mode=='barrier_fail' else 1}
   self.stubs={(CODE+sy[n])&~1:f for n,f in stubs.items()}
   self.stubs.update({0xc3514ec:lambda:self.alloc(self.reg(0)),0xc3511b0:lambda:self.alloc(self.reg(0)),0xc34cc7c:self.free,0xc721410:self.memcpy,0xc720f08:self.memset})
   u.hook_add(UC_HOOK_CODE,self.hook)
  def reg(self,i):return self.u.reg_read(regs[i])
  def put(self,p,*v):self.u.mem_write(p,struct.pack('<'+'I'*len(v),*v))
  def word(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
  def get(self,k,b=B):return self.word(b+self.layout[k])
  def set(self,k,v,b=B):self.put(b+self.layout[k],v)
  def alloc(self,n):
   p=self.heap;self.heap+=(n+7)&~7;assert self.heap<0x3c550000;self.u.mem_write(p,bytes(n));self.allocations[p]=n;return p
  def free(self):
   p=self.reg(0);assert p in self.allocations and p not in self.freed,hex(p);self.freed.append(p);self.u.mem_write(p,bytes([0xdd])*self.allocations[p]);return 0
  def memcpy(self):self.u.mem_write(self.reg(0),bytes(self.u.mem_read(self.reg(1),self.reg(2))));return self.reg(0)
  def memset(self):self.u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2));return self.reg(0)
  def rpc(self):
   client,p,cmd=self.reg(0),self.reg(1),self.reg(2);assert client==C and self.word(C+4)==CLIENT;self.calls.append(cmd);cookie=self.word(p+60)
   if self.mode=='create_service_fail' and cmd==0x74:self.server.fail_create=True
   if cmd==0x75:
    self.deletes+=1
    if self.mode=='delete_transport_fail' and self.deletes==1:return 13
    if self.mode=='guard_delete_fail' and self.deletes==2:self.u.mem_write(p+12,b'\2');return 0
   status,remote=self.server.rpc(cmd,cookie)
   if self.mode=='create_transport_fail' and cmd==0x74:return 13
   self.u.mem_write(p+12,bytes([status]));self.put(p+20,remote);return 0
  def ret(self,v=0):self.u.reg_write(ar.UC_ARM_REG_R0,v);self.u.reg_write(ar.UC_ARM_REG_PC,self.u.reg_read(ar.UC_ARM_REG_LR))
  def hook(self,u,pc,size,data):
   if pc==self.end:u.emu_stop()
   elif pc==CALLBACK:
    self.callback_count+=1
    if self.delivery_code==0x122:self.set('terminal',1)
    self.ret()
   elif pc in self.stubs:self.ret(self.stubs[pc]())
  def call(self,name,args=()):
   pc=CODE+sy[name] if isinstance(name,str) else name;self.u.reg_write(ar.UC_ARM_REG_SP,0x3c5ef000);self.u.reg_write(ar.UC_ARM_REG_LR,STOP|1)
   for reg,value in zip(regs,args[:4]):self.u.reg_write(reg,value)
   for i,value in enumerate(args[4:]):self.put(0x3c5ef000+i*4,value)
   self.u.emu_start(pc|1,0,count=3000000);assert self.u.reg_read(ar.UC_ARM_REG_PC)==STOP,hex(self.u.reg_read(ar.UC_ARM_REG_PC));return self.reg(0)
  def decode(self,packet):
   self.delivery_code=struct.unpack_from('<I',packet)[0];self.u.mem_write(PACK,packet);return self.call(0xc46576c,(0,0xffffffff,CLIENT,PACK,0))
  def queue(self,code,cookie):
   q=self.alloc(720);head=CLIENT+76;tail=self.word(head);self.put(q,tail,head,CLIENT);self.put(tail+4,q);self.put(head,q);self.put(q+12,code);self.put(q+12+60,cookie,0);return q
  def clear_queue_model(self):self.put(CLIENT+76,CLIENT+76,CLIENT+76)
  def run(self):
   # Execute real compact exact guards, including the new retirement surface.
   assert self.call('nsr_guards')==1
   original=bytes(self.u.mem_read(0xc473eb4,1));self.u.mem_write(0xc473eb4,bytes([original[0]^1]));assert self.call('nsr_guards')==0;self.u.mem_write(0xc473eb4,original)
   result=self.call('nsg3_create',(U,C,B,CB));old_token=self.get('token');old_node=self.get('node')
   if self.mode in ['create_service_fail','create_transport_fail']:
    if self.mode=='create_transport_fail':assert result!=0 and old_token not in self.freed and old_node not in self.freed and self.get('state')==6
    else:assert result!=0 and self.get('token')==0 and len(self.freed)==2
    return dict(case=self.mode,free_count=len(self.freed),state=self.get('state'))
   assert result==0 and self.get('state')==2
   assert self.call('nsg3_bind_ui',(U,C,B))==0;head=self.get('head');assert self.word(head+8)==1
   foreign=foreign_node=0
   if self.mode=='foreign_owner':
    foreign=self.alloc(16);self.put(foreign,CLIENT,CB,0,0);self.call(0xc465b74,(head,foreign));foreign_node=self.word(head)
   self.set('ever_attempted',1);self.server.disconnect_pause(self.get('remote'))
   packet=self.server.wire[-1][1];decoded=self.decode(packet);assert decoded==0 and self.get('terminal')==1,(decoded,self.callback_count,packet[:4].hex(),packet[60:68].hex(),hex(old_token),self.get('state'))
   result=self.call('nsg3_retire_remote',(U,C,B,GUARD,SYNC))
   if self.mode in ['delete_transport_fail','guard_delete_fail','barrier_fail']:
    assert result!=0;assert self.call('nsg3_collect_ui',(U,C,B,GUARD,0))!=0
    assert old_token not in self.freed and old_node not in self.freed
    return dict(case=self.mode,free_count=len(self.freed),state=self.get('state'),guard_state=self.get('state',GUARD))
   assert result==0 and self.get('state')==4 and self.get('state',GUARD)==4 and self.get('barrier_ok')==1
   gt,gn=self.get('token',GUARD),self.get('node',GUARD)
   if self.mode in ['pending_own','unknown_queue']:
    self.queue(0x128 if self.mode=='pending_own' else 0xdead,old_token)
    assert self.call('nsg3_collect_ui',(U,C,B,GUARD,0))==1 and not self.freed
    self.clear_queue_model()
   if self.mode=='client_changed':
    self.live=False;assert self.call('nsg3_collect_ui',(U,C,B,GUARD,0))!=0 and not self.freed;return dict(case=self.mode,free_count=0)
   if self.mode=='wrong_ui':
    self.owner=False;assert self.call('nsg3_collect_ui',(U,C,B,GUARD,0))!=0 and not self.freed;return dict(case=self.mode,free_count=0)
   if self.mode=='foreign_owner':self.queue(0x128,foreign)
   assert self.call('nsg3_collect_ui',(U,C,B,GUARD,0))==0
   assert set(self.freed)=={old_token,old_node,gt,gn} and self.get('token')==0 and self.get('node')==0 and self.get('state')==5
   assert head not in self.freed and self.word(CLIENT+200)==head
   if self.mode=='foreign_owner':assert self.word(head+8)==1 and self.word(head+4)==foreign_node and foreign not in self.freed and foreign_node not in self.freed
   else:assert self.word(head+8)==0 and self.word(head)==head and self.word(head+4)==head
   late=bytearray(708);struct.pack_into('<I',late,0,0x128);struct.pack_into('<I',late,60,old_token)
   count=self.callback_count;assert self.decode(bytes(late))==0x32 and self.callback_count==count,'retired cookie must no longer dispatch'
   return dict(case=self.mode,free_count=len(self.freed),state=self.get('state'),shared_head_preserved=True,stale_cookie_rejected=True,server_mutex_fence='guard waits for same profile mutex' in self.server.trace)
 results=[]
 for mode in ['normal','foreign_owner','pending_own','unknown_queue','create_service_fail','create_transport_fail','delete_transport_fail','guard_delete_fail','barrier_fail','client_changed','wrong_ui']:
  row=Model(mode).run();results.append(row);print(mode,'PASS',row['free_count'])
 (R/'build/resource-verification.json').write_text(json.dumps(dict(hardware_tested=False,actual='Compiled resource C; exact client GATT decoder and list operations; exact server profile create/disconnect/delete/destructors and common mutex interleaving',modeled='Initialized RAM, SDK/UI identity, OS allocator/scheduler/mutex/barrier/socket/SAL; no hardware release inferred',cases=results),indent=2)+'\n')
if __name__=='__main__':main()
