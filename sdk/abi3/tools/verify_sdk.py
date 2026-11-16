"""G3 real ARM consumer + frozen G2 SDK + retail wrappers/decoder; OS/radio modeled."""
import argparse,ast,hashlib,json,struct,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--research',type=Path,required=True);a=p.parse_args();sys.path.insert(0,str(a.research/'python-deps'))
 from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
 from unicorn import arm_const as ar
 from prove_attribute_rom import AttributeRom
 from prove_retirement_rom import ServerRom
 ap=(a.research/'band11/179-vela_ap.bin').read_bytes();assert hashlib.sha256(ap).hexdigest()=='307adabe0ff61c77be73d045a8260e279cddd131c7a535a2678eb34199278a2b'
 sy=json.loads((R/'build/model-symbols.json').read_text());raw=(R/'build/model.bin').read_bytes()
 names=json.loads((R/'reference/g2-model-field-names.json').read_text())
 CODE=0x1c500800;ROOT=0x3c518000;CLIENT=0x3c510000;PACK=0x3c512000;TIMER=0x3c513000;STOP=0x20000000
 canonical=bytes.fromhex('91d10001d0b24cf69136f16db55b6e11')
 regs=[ar.UC_ARM_REG_R0,ar.UC_ARM_REG_R1,ar.UC_ARM_REG_R2,ar.UC_ARM_REG_R3]
 class Model:
  def __init__(self,mode):
   self.mode=mode;self.server=ServerRom(a.research,0x3c560000);self.u=u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);self.attr=AttributeRom(a.research)
   for base,size in [(0xc0c0000,0xc00000),(0x2c0c0000,0xc00000),(0x1c500000,0x10000),(0x3c500000,0x100000),(STOP,0x1000),(0x20080000,0x60000)]:u.mem_map(base,size)
   u.mem_write(0xc0c0000,ap);u.mem_write(0x2c0c0000,ap);u.mem_write(CODE,raw)
   self.layout=dict(zip(names,struct.unpack('<'+'I'*len(names),u.mem_read(CODE+sy['g1_test_layout'],len(names)*4))))
   self.report_off=sy.get('unused',0);self.report_off=self.word(CODE+sy['g4_fixture_layout']);self.error_off=self.word(CODE+sy['g4_fixture_layout']+4);self.done_off=self.word(CODE+sy['g4_fixture_layout']+8)
   self.tasks=[];self.timers={TIMER:dict(cb=CODE+sy['owned_callback'],data=ROOT,active=True)};self.queue=[];self.heap=0x3c530000;self.owners=[];self.current=0;self.running=None;self.stopped=False;self.frees=[];self.commands=[];self.injected=False
   self.put(ROOT,0x47340002,1,TIMER);self.put(CLIENT+76,CLIENT+76,CLIENT+76);self.put(0x200bd108,24,0,0);self.allocations={};self.sems={};self.async_handles={};self.async_queue=[];self.hung=False;self.worker_steps=0
   def table():return {
    'nsp_valid':lambda:1,'nsp_guards':lambda:1,'nsr_guards':lambda:1,'nsp_ui_capture':self.capture,'nsp_ui_matches':lambda:1,'nsp_worker_identity':self.identity,'nsr_borrow':self.borrow,'nsr_live':lambda:1,'nsr_callback_context':self.callback_context,
    'nsp_alloc':lambda:self.alloc(self.reg(0)),'nsp_free':self.free,'nsp_timer':self.timer,'nsp_timer_data':lambda:self.timers[self.reg(0)]['data'],'nsp_timer_delete':self.delete_timer,'nsp_spawn':self.spawn,'nsp_pause':self.pause,'nsp_exit_proof':self.exit_proof,'nsp_join':self.join,'nsr_barrier':lambda:1,'nsr_rpc':self.rpc}
   self.stubs={(CODE+sy[n])&~1:f for n,f in table().items()}
   self.stubs.update({0xc3514ec:lambda:self.alloc(self.reg(0)),0xc3511b0:lambda:self.alloc(self.reg(0)),0xc34cc7c:self.free,0xc721410:self.memcpy,0xc720f08:self.memset,0xc46a18c:self.rpc})
   self.stubs.update({0xc8bbb10:self.sem_init,0xc3f5120:self.sem_post,0xc3f513c:self.sem_wait,0xc3f5104:lambda:0,0xc3f455c:self.async_init,0xc3f5668:self.async_send})
   u.hook_add(UC_HOOK_CODE,self.hook)
  def reg(self,i):return self.u.reg_read(regs[i])
  def put(self,p,*v):self.u.mem_write(p,struct.pack('<'+'I'*len(v),*v))
  def word(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
  def get(self,n,g=None):return self.word((g or self.current)+self.layout[n])
  def alloc(self,n):
   p=self.heap;self.heap+=(n+7)&~7;assert self.heap<0x3c550000;self.u.mem_write(p,bytes(n));self.allocations[p]=n
   if n==self.layout['size']:self.owners.append(p)
   return p
  def free(self):
   p=self.reg(0);assert p in self.allocations and p not in self.frees,hex(p);self.frees.append(p);self.u.mem_write(p,bytes([0xdd])*self.allocations[p]);return 0
  def sem_init(self):self.sems[self.reg(0)]=0;return 0
  def sem_post(self):self.sems[self.reg(0)]+=1;return 0
  def sem_wait(self):
   if self.sems[self.reg(0)]:self.sems[self.reg(0)]-=1;return 0
   self.running['waiting']=self.reg(0);self.u.emu_stop();return None
  def async_init(self):self.async_handles[self.reg(1)]=self.reg(2);return 0
  def async_send(self):
   if self.reg(0) not in self.async_queue:self.async_queue.append(self.reg(0))
   return 0
  def memcpy(self):self.u.mem_write(self.reg(0),bytes(self.u.mem_read(self.reg(1),self.reg(2))));return self.reg(0)
  def memset(self):self.u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2));return self.reg(0)
  def capture(self):self.put(self.reg(0),1,2,48,0,0);return 1
  def identity(self):self.put(self.reg(1),self.running['tid']);return 1
  def borrow(self):self.put(self.reg(1),3,CLIENT,4,5,6);return 1
  def callback_context(self):self.put(self.reg(2),48);return 1
  def timer(self):
   p=0x3c51b000+64*len(self.timers);self.timers[p]=dict(cb=self.reg(0),data=self.reg(1),active=True);return p
  def delete_timer(self):self.timers[self.reg(0)]['active']=False;return 0
  def spawn(self):
   t=dict(tid=1001+len(self.tasks),g=self.reg(2),entry=self.reg(1),context=None,returned=False);self.tasks.append(t);self.put(self.reg(0),t['tid']);return 0
  def pause(self):self.u.emu_stop();return 0
  def exit_proof(self):
   t=next(t for t in self.tasks if t['tid']==self.reg(1));return 1 if self.mode=='missing_exit' or not t['returned'] else 0
  def join(self):
   t=next(t for t in self.tasks if t['tid']==self.reg(0));assert t['returned'];self.put(self.reg(1),0x4738a739);return 1 if self.mode=='join_fail' else 0
  def hook(self,u,pc,size,data):
   if pc==STOP:self.stopped=True;u.emu_stop()
   elif pc in self.stubs:
    v=self.stubs[pc]();
    if v is not None:u.reg_write(ar.UC_ARM_REG_R0,v);u.reg_write(ar.UC_ARM_REG_PC,u.reg_read(ar.UC_ARM_REG_LR))
  def call(self,pc,args=()):
   self.stopped=False;self.u.reg_write(ar.UC_ARM_REG_SP,0x3c5ff000);self.u.reg_write(ar.UC_ARM_REG_LR,STOP|1)
   for reg,v in zip(regs,args):self.u.reg_write(reg,v)
   self.u.emu_start(pc|1,0,count=2000000);assert self.stopped,('call budget',hex(self.u.reg_read(ar.UC_ARM_REG_PC)))
  def event(self,code,body=b'',cookie=None):
   p=bytearray(708);self.put_bytes(p,0,code);self.put_bytes(p,60,cookie or self.get('handle'));p[68:68+len(body)]=body;self.queue.append(('gatt',bytes(p),self.current))
  def put_bytes(self,p,off,v):struct.pack_into('<I',p,off,v)
  def rpc(self):
   client,p,cmd=self.reg(0),self.reg(1),self.reg(2)
   if client!=CLIENT:client=self.word(client+4)
   assert client==CLIENT
   if self.mode=='rpc_held' and cmd==0x74:
    self.running['rpc_waiting']=True;self.u.emu_stop();return None
   self.commands.append([self.current,cmd]);b=bytes(self.u.mem_read(p,708));self.u.mem_write(p+12,bytes(36))
   if cmd==0xed:
    self.put(p+12,0xa123+len(self.tasks),0);self.queue.append(('scan_start',0,self.current))
    if self.mode!='no_peer':self.queue.append(('scan_result',0,self.current))
   elif cmd==0xee:self.queue.append(('scan_stop',0,self.current))
   elif cmd in [0x74,0x75]:
    status,remote=self.server.rpc(cmd,struct.unpack_from('<I',b,60)[0]);self.u.mem_write(p+12,bytes([status]));self.put(p+20,remote,0)
   elif cmd==0x76:
    if self.mode=='connect_fail':self.u.mem_write(p+12,b'\2');return 0
    if self.mode=='cancel_connect':self.call_close_later=self.current;return 0
    assert b[68:74]==b'ABCDEF';self.event(0x121,b'ABCDEF');self.event(0x124,struct.pack('<IB',514,0))
   elif cmd==0x77:
    remote=struct.unpack_from('<I',b,60)[0];self.server.disconnect_pause(remote);self.queue.append(('gatt',self.server.wire[-1][1],self.current))
   elif cmd==0x78:self.event(0x123,struct.pack('<HH',10,14)+b'\x10\0\0\0'+canonical+bytes(1));self.event(0x123,bytes(25))
   elif cmd==0x79:
    status,desc=self.attr.query(struct.unpack_from('<H',b,68)[0]);self.u.mem_write(p+12,bytes([status]));self.u.mem_write(p+20,desc)
   elif cmd==0x7e:self.event(0x127,struct.pack('<HBB',12,1 if self.mode=='subscribe_fail' else 0,1))
   elif cmd==0x7b:
    if self.mode=='peer_disconnect':
     remote=struct.unpack_from('<I',b,60)[0];self.server.disconnect_pause(remote);self.queue.append(('gatt',self.server.wire[-1][1],self.current));return 0
    v=bytearray(521);struct.pack_into('<HH',v,0,12,8);
    if self.mode in ['read_cancel_after_close','read_error_before_close'] and len(self.owners)==2:v[-1]=1;struct.pack_into('<H',v,2,0)
    v[4:12]=b'G1-READ!';self.event(0x125,bytes(v))
   elif cmd==0x7c:
    n=struct.unpack_from('<H',b,70)[0];payload=b[72:72+n];bad=payload==b'G4-FAIL!'
    self.event(0x126,struct.pack('<HB',12,1 if bad or self.mode=='write_fail' else 0))
    if not bad and self.mode!='no_notify':self.event(0x128,struct.pack('<HH',12,n)+payload)
   else:raise AssertionError(hex(cmd))
   return 0
  def deliver(self):
   if not self.queue:return
   kind,p,g=self.queue[0]
   if self.mode=='read_cancel_after_close' and kind=='gatt' and struct.unpack_from('<I',p)[0]==0x125 and g==self.owners[1] and not self.get('stop',g):return
   self.queue.pop(0)
   if kind=='gatt':self.u.mem_write(PACK,p);self.put(0x3c5ff000,0);self.call(0xc46576c,(0,0xffffffff,CLIENT,PACK))
   else:
    token=self.get('scan_token',g);pkt=bytearray(708)
    code={'scan_start':0x168,'scan_stop':0x169,'scan_result':0x167}[kind];self.put_bytes(pkt,0,code)
    self.put_bytes(pkt,340 if kind=='scan_start' else 356 if kind=='scan_stop' else 60,token)
    if kind=='scan_result':pkt[68:81]=b'ABCDEF'+bytes([0,0,1,0,18,0,0]);pkt[81:99]=bytes([17,7])+canonical[::-1]
    self.u.mem_write(PACK,bytes(pkt));self.call(0xc463f7a,(0,0xffffffff,CLIENT,PACK))
  def run(self):
   for tick in range(20000):
    self.put(ROOT+4,tick+1)
    for t in list(self.tasks):
     if t['returned'] or t.get('rpc_waiting') or (t.get('waiting') and not self.sems[t['waiting']]):continue
     self.current=t['g'];self.running=t;self.stopped=False
     if t['context'] is None:
      self.u.reg_write(ar.UC_ARM_REG_SP,0x3c5d0000-(t['tid']-1001)*0x3000);self.u.reg_write(ar.UC_ARM_REG_LR,STOP|1);self.u.reg_write(ar.UC_ARM_REG_R0,t['g']);self.u.reg_write(ar.UC_ARM_REG_PC,t['entry'])
     else:self.u.context_restore(t['context'])
     t['waiting']=None;self.worker_steps+=1
     self.u.emu_start(self.u.reg_read(ar.UC_ARM_REG_PC)|1,0,count=2000000)
     if self.stopped:t['returned']=True
     t['context']=self.u.context_save()
    self.deliver()
    if getattr(self,'call_close_later',0):
     self.call(CODE+sy['ns3_close'],(self.call_close_later,));self.call_close_later=0
    for _ in range(4):
     if not self.async_queue:break
     handle=self.async_queue.pop(0);self.call(self.async_handles[handle],(handle,))
    if tick%10==0:
     for timer,info in list(self.timers.items()):
      if info['active']:self.call(info['cb'],(timer,))
    if not self.timers[TIMER]['active']:break
   report=list(struct.unpack('<26I',self.u.mem_read(ROOT+self.report_off,104)))
   if self.mode in ['normal','read_cancel_after_close']:
    assert report[1]==0 and report[3]==4 and report[23]==1,report
    assert report[4:8]==[4,4,0,0] and report[11:13]==[32,32],report
    assert report[13]==report[14] and report[13]>=20 and report[18:20]==[1,8],report
    assert all(g in self.frees for g in self.owners)
    assert self.server.word(self.server.word(0x200be700)+8)==0
   elif self.mode=='rpc_held':assert report[1]==111 and report[23]==1 and any(t.get('rpc_waiting') for t in self.tasks) and any(g not in self.frees for g in self.owners),report
   elif self.mode=='cancel_connect':assert report[1]!=0 or report[3]<4,report
   elif self.mode in ['missing_exit','join_fail']:assert report[23]==1 and not self.frees and report[5]==0 and report[6]==1 and report[15:17]==[0,0],report
   elif self.mode=='read_error_before_close':assert report[1]==53 and report[3]==1 and report[4:8]==[2,2,0,0] and report[23]==1,report
   else:assert report[1]!=0 and report[3]<4,report
   return dict(case=self.mode,report=report,owners=len(self.owners),commands=[[hex(g),hex(c)] for g,c in self.commands],free_count=len(self.frees),worker_dispatches=self.worker_steps)
 results=[]
 for mode in ['normal','read_cancel_after_close','read_error_before_close','no_peer','connect_fail','subscribe_fail','write_fail','peer_disconnect','no_notify','cancel_connect','rpc_held','missing_exit','join_fail']:
  row=Model(mode).run();results.append(row);print(mode,'PASS',row['report'])
 rec=dict(hardware_tested=False,actual='Compiled G4R2 public consumer + ABI3 archive, exact client GATT+scan decoders/list, exact server create/disconnect/delete under shared mutex, exact sparse attribute lookup',modeled='OS allocator/UV async/semaphore/timer/thread/exit-proof/join/barrier, scheduler, transport/radio; no hardware inference',sdk_library_sha256=hashlib.sha256((R/'sdk-export/lib/libns_link_v3_band11_candidate.a').read_bytes()).hexdigest(),cases=results,artifacts={n:hashlib.sha256((R/'build'/n).read_bytes()).hexdigest() for n in ['g1.bin','bootstrap.bin','model.bin']})
 (R/'build/verification.json').write_text(json.dumps(rec,indent=2)+'\n')
if __name__=='__main__':main()
