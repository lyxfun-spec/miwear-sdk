"""Exact retail profile lifecycle; initialized RAM/OS/radio are explicit models."""
from pathlib import Path
import argparse,hashlib,json,struct,sys

class ServerRom:
 def __init__(self,research,heap_base=0x3c530000):
  sys.path.insert(0,str(Path(research)/'python-deps'))
  from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
  from unicorn import arm_const as ar
  self.ar=ar;self.u=u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  ap=(Path(research)/'band11/179-vela_ap.bin').read_bytes();assert hashlib.sha256(ap).hexdigest()=='307adabe0ff61c77be73d045a8260e279cddd131c7a535a2678eb34199278a2b'
  for b,n in [(0xc0c0000,0xc00000),(0x2c0c0000,0xc00000),(0x20080000,0x60000),(0x3c500000,0x100000),(0x20000000,0x1000)]:u.mem_map(b,n)
  u.mem_write(0xc0c0000,ap);u.mem_write(0x2c0c0000,ap)
  self.regs=[ar.UC_ARM_REG_R0,ar.UC_ARM_REG_R1,ar.UC_ARM_REG_R2,ar.UC_ARM_REG_R3]
  self.heap=heap_base;self.allocations={};self.freed=[];self.wire=[];self.mutex=False;self.trace=[];self.next_id=0;self.end=0x20000000;self.paused=False;self.blocked=False;self.pause_callback=False;self.handler_context=None;self.fail_create=False
  self.put(0x200bdc8c+9*8,0x3c501000);self.put(0x3c501000+0x34,0xc46fdf5)
  self.u.mem_write(0x200be6f8,b'\1');self.put(0x200be6fc,0x3c502000)
  u.hook_add(UC_HOOK_CODE,self.hook)
  self.call(0xc4655b2,(0xc475bc5,));self.put(0x200be700,self.reg(0))
 def reg(self,i):return self.u.reg_read(self.regs[i])
 def put(self,p,*v):self.u.mem_write(p,struct.pack('<'+'I'*len(v),*v))
 def word(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
 def alloc(self,n):
  p=self.heap;self.heap+=(n+7)&~7;assert self.heap<0x3c5b0000;self.u.mem_write(p,bytes(n));self.allocations[p]=n;return p
 def ret(self,v=0):self.u.reg_write(self.ar.UC_ARM_REG_R0,v);self.u.reg_write(self.ar.UC_ARM_REG_PC,self.u.reg_read(self.ar.UC_ARM_REG_LR))
 def hook(self,u,pc,size,data):
  if pc==self.end:u.emu_stop();return
  if pc in [0xc3514ec,0xc3511b0]:self.ret(self.alloc(self.reg(0)))
  elif pc==0xc35118c:self.ret(0 if self.fail_create else self.alloc(self.reg(0)*self.reg(1)))
  elif pc==0xc34cc7c:
   p=self.reg(0)
   if p:assert p in self.allocations and p not in self.freed,hex(p);self.freed.append(p)
   self.ret()
  elif pc==0xc721410:self.u.mem_write(self.reg(0),bytes(self.u.mem_read(self.reg(1),self.reg(2))));self.ret(self.reg(0))
  elif pc==0xc720f08:self.u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2));self.ret(self.reg(0))
  elif pc==0xc34f622:
   assert self.reg(0)==0x200be704
   if self.mutex:self.blocked=True;self.trace.append('guard waits for same profile mutex');u.emu_stop()
   else:self.mutex=True;self.trace.append('profile mutex acquired');self.ret()
  elif pc==0xc354818:assert self.reg(0)==0x200be704 and self.mutex;self.mutex=False;self.trace.append('profile mutex released');self.ret()
  elif pc in [0xc3547e8,0xc359de0,0xc466040]:self.ret()
  elif pc==0xc465ff4:self.next_id+=1;self.ret(self.next_id)
  elif pc==0xc470b24:self.ret(0)
  elif pc==0xc471890:
   assert bytes(self.u.mem_read(self.reg(0),6))==bytes(6),'only disconnected/unconnected handles are retired'
   self.trace.append('SAL disconnect of empty address (separate zero-address ROM proof)');self.ret()
  elif pc==0xc46b888:
   code=self.reg(2);self.put(self.reg(1),code) # Equivalent transport header store at c46b8a6.
   packet=bytes(self.u.mem_read(self.reg(1),708));self.wire.append((code,packet));self.trace.append('wire '+hex(code));self.ret()
   if self.pause_callback and code==0x122:self.paused=True;self.pause_callback=False;u.emu_stop()
 def call(self,pc,args=(),end=0x20000000,stack=0x3c5ff000):
  self.end=end;self.paused=self.blocked=False
  self.u.reg_write(self.ar.UC_ARM_REG_SP,stack);self.u.reg_write(self.ar.UC_ARM_REG_LR,0x20000001)
  for reg,v in zip(self.regs,args):self.u.reg_write(reg,v)
  self.u.emu_start(pc|1,0,count=400000)
  assert self.paused or self.blocked or self.u.reg_read(self.ar.UC_ARM_REG_PC)==end,hex(self.u.reg_read(self.ar.UC_ARM_REG_PC))
 def rpc(self,cmd,cookie):
  p=self.alloc(708);self.put(p,cmd);self.put(p+60,cookie)
  self.u.reg_write(self.ar.UC_ARM_REG_R4,p);self.u.reg_write(self.ar.UC_ARM_REG_R5,0x3c52f000)
  self.call(0xc46e482 if cmd==0x74 else 0xc46e4e4,end=0xc46d3f0)
  if self.blocked:
   blocked=self.u.context_save();assert self.handler_context is not None
   self.u.context_restore(self.handler_context);self.end=0x20000000;self.u.emu_start(self.u.reg_read(self.ar.UC_ARM_REG_PC)|1,0,count=400000)
   assert not self.mutex and self.u.reg_read(self.ar.UC_ARM_REG_PC)==self.end
   self.handler_context=None;self.u.context_restore(blocked);self.end=0xc46d3f0;self.blocked=False
   self.u.emu_start(self.u.reg_read(self.ar.UC_ARM_REG_PC)|1,0,count=400000);assert self.u.reg_read(self.ar.UC_ARM_REG_PC)==self.end
  return self.u.mem_read(p+12,1)[0],self.word(p+20)
 def disconnect_pause(self,connection):
  self.u.mem_write(connection+44,b'ABCDEF');self.u.mem_write(connection+8,b'\2')
  msg=self.alloc(16);self.u.mem_write(msg,b'\0ABCDEF\0\0'+bytes(7));self.pause_callback=True
  self.call(0xc473930,(msg,),stack=0x3c5fd000);assert self.paused and self.mutex
  self.handler_context=self.u.context_save()
 def disconnect_complete(self,connection):
  self.disconnect_pause(connection);self.pause_callback=False
  self.end=0x20000000;self.u.context_restore(self.handler_context);self.u.emu_start(self.u.reg_read(self.ar.UC_ARM_REG_PC)|1,0,count=400000);self.handler_context=None
  assert not self.mutex

def zero_address_proof(research):
 """Execute the actual stack selector with unrelated active peer fixtures."""
 s=ServerRom(research);u=s.u;a=s.ar;guard_payload=s.alloc(8);u.mem_write(guard_payload,bytes(8))
 # c87da34 only continues if the modeled stack enable flag is present.
 s.put(0x200bd900+0x18c,0x400)
 p1=s.alloc(32)+4;p2=s.alloc(32)+4;s.put(0x200bd900+0x1b8,p1);s.put(p1-4,p2);s.put(p2-4,0)
 u.mem_write(p1,b'ABCDEF\0\0');u.mem_write(p2,b'UVWXYZ\0\0')
 calls=[]
 from unicorn import UC_HOOK_CODE
 def fail(uc,pc,size,data):
  if pc in [0xc87a388,0xc874b84]:calls.append(hex(pc));raise AssertionError('zero address affected unrelated channel')
 u.hook_add(UC_HOOK_CODE,fail)
 s.call(0xc87da34,(0,0,guard_payload))
 assert not calls and guard_payload in s.freed
 return dict(unrelated_peer_addresses=['ABCDEF','UVWXYZ'],stack_disconnect_calls=calls,request_payload_freed=True,actual='Stack c87da34/c87d034/c887464 and zero-address comparison c86f81c/c8aa864; initialized channel RAM modeled')
def main():
 p=argparse.ArgumentParser();p.add_argument('--research',type=Path,required=True);a=p.parse_args()
 s=ServerRom(a.research);status,old=s.rpc(0x74,0x3c511000);assert status==0
 status,foreign=s.rpc(0x74,0x3c512000);assert status==0;s.u.mem_write(foreign+44,b'UVWXYZ')
 s.disconnect_pause(old)
 status,guard=s.rpc(0x74,0x3c513000);assert status==0 and not s.mutex and 'guard waits for same profile mutex' in s.trace
 assert bytes(s.u.mem_read(old+44,6))==bytes(6)
 assert s.rpc(0x75,old)[0]==0 and s.rpc(0x75,guard)[0]==0
 assert old in s.freed and guard in s.freed and foreign not in s.freed
 assert s.word(s.word(0x200be700)+8)==1
 assert [x[0] for x in s.wire]==[0x74,0x74,0x122,0x74,0x75,0x75]
 z=zero_address_proof(a.research)
 out=Path(__file__).resolve().parents[1]/'build/retirement-rom-proof.json';out.parent.mkdir(exist_ok=True)
 out.write_text(json.dumps(dict(hardware_tested=False,actual='Exact profile create/global-mutex, disconnect message/address clear, delete/destructors/list operations, socket callback+reply writers; zero-address selector',modeled='Initialized manager/registry/channel RAM; allocator/index/mutex/OS/SAL queue/send; mutex blocking interleaving modeled at exact call boundary; RPC packet storage retained',trace=s.trace,foreign_connection_preserved=True,old_and_guard_service_objects_freed=True,zero_address=z),indent=2)+'\n')
 print('Exact ROM retirement: in-flight callback blocks guard create until address clear; old+guard removed, foreign preserved; zero address touches no unrelated channel')
if __name__=='__main__':main()
