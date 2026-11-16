"""Execute NS2 ARM backends with explicit scheduler/kernel/timer models."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
R=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--research',type=Path,required=True);args=parser.parse_args()
    old=args.research.resolve();sys.path[:0]=[str(old/'python-deps'),str(R/'tests/loader')]
    from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
    from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
    from check_native_module import run_case
    regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]
    STOP=0x20000000
    class Machine:
        def __init__(self):
            self.uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);self.stubs={};self.stopped=False
            self.uc.hook_add(UC_HOOK_CODE,self.hook)
        def reg(self,n):return self.uc.reg_read(regs[n])
        def word(self,a):return struct.unpack('<I',self.uc.mem_read(a,4))[0]
        def hook(self,u,a,n,data):
            if a==STOP:self.stopped=True;u.emu_stop()
            elif a in self.stubs:
                v=self.stubs[a](self)
                if v is not None:u.reg_write(UC_ARM_REG_R0,v)
                u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
    class Harness:
        def __init__(self,target,mode,t):
            self.target=target;self.mode=mode;self.t=t;self.workers=[];self.allocations={};self.frees=[];self.reports=[]
            self.cursor=0x3c410000;self.timer=0x3c301000;self.callback=0;self.runtime=0;self.timer_live=False;self.join_calls=0;self.join_while_alive=0
            self.band=target=='band11';self.current=0x200b0364 if self.band else 0x3c2abec0
            self.uitcb=0x20110000 if self.band else 0x3c300000
            self.group=0x20111000 if self.band else 0x3c302000
            self.table=0x20112000 if self.band else 0x3c303000
            self.uiid=48 if self.band else 77
            self.a={'attr':0xc348784,'stack':0xc3487b6,'thread':0xc350c54,'pause':0xc34cb8c,'join':0xc359764,'alloc':0xc3514ec,'free':0xc34cc7c,'timer':0xc3abc88,'delete':0xc3abdd8} if self.band else {'attr':0x2c34aacc,'stack':0x2c34aafe,'thread':0x2c351b7c,'pause':0x2c356348,'join':0x2c336940,'alloc':0x2c35a16c,'free':0x2c358260,'timer':0x2c4f7488,'delete':0x2c4f6f78}
            self.put(self.uitcb+8,self.group);self.put(self.uitcb+(44 if self.band else 12),self.uiid);self.put(self.current,self.uitcb)
            self.put(self.group,42,42)
            self.put(0x200b0378 if self.band else 0x3c2aba18,256)
            self.put(0x200ba820 if self.band else 0x3c2aba30,self.table)
            t.stubs.update({self.a['attr']:self.attr,self.a['stack']:self.stack,self.a['thread']:self.spawn,
                self.a['pause']:self.pause,self.a['alloc']:lambda t:self.alloc(t.reg(0)),self.a['free']:self.free,
                self.a['timer']:self.create_timer,self.a['delete']:self.delete_timer})
            if self.band:
                self.put(0x200b2330,0x3c356b40);self.put(0x3c356b5c,0x3c300000,0x3c700000)
                self.put(0x200b0070,0x2010d9e0);self.put(0x2010d9fc,0x2010db00,0x20170000)
                # Existing foreign timer must survive this runtime's withdrawal.
                self.foreign=0x3c300800;self.put(self.foreign+8,0xc0c181f,0)
                self.put(0x200bd108,24,self.foreign,self.foreign)
                t.stubs.update({self.a['join']:self.band_join,0xc92e6da:self.cache,0xc91ca02:lambda t:0,0xc0c181e:lambda t:0})
            else:
                # Loader allocations use live-address-class fixtures too.
                t.stubs.update({0x2c35a12c:lambda t:self.alloc(t.reg(0)),0x2c35a0c8:lambda t:self.alloc(t.reg(1),t.reg(0)),
                    0x2c34e494:lambda t:0,0x2c34f2e8:lambda t:0,0x2c8d9750:lambda t:0,0x2c8d96f0:lambda t:0,
                    0x2c3356c8:lambda t:0,0x2c6a74c4:self.opened,0x2c34c35c:self.printed})
                # Keep exact pthread_join / list lookup / unlink instructions.
                t.uc.hook_add(UC_HOOK_CODE,self.observe_join)
                if mode=='join_fail':t.stubs[self.a['join']]=self.fail_join
                self.row=[]
        def put(self,a,*v):self.t.uc.mem_write(a,struct.pack('<'+'I'*len(v),*v))
        def alloc(self,n,align=8):
            self.cursor=(self.cursor+align-1)&-align;p=self.cursor;self.cursor+=max(n,8)
            assert self.cursor<0x3c4d0000;self.allocations[p]={'bytes':n,'kind':'other'}
            self.t.uc.mem_write(p,bytes(n));return p
        def attr(self,t):self.put(t.reg(0),0x10064,0,2048);return 0
        def stack(self,t):assert t.reg(1)==8192;self.put(t.reg(0)+8,8192);return 0
        def spawn(self,t):
            out,attr,entry,arg=[t.reg(i) for i in range(4)];assert [t.word(attr+4*i) for i in range(3)]==[0x10064,0,8192]
            self.allocations[arg]['kind']='session'
            if self.mode=='create_fail':return 12
            assert not any(not w['joined'] for w in self.workers),'New session before previous join'
            tid=1001+len(self.workers);self.put(out,tid)
            tcb=(0x20120000 if self.band else 0x3c310000)+len(self.workers)*256
            self.put(tcb+8,self.group);self.put(tcb+(44 if self.band else 12),tid)
            self.put(self.table+(tid&255)*4,tcb)
            main=t.uc.context_save();t.uc.reg_write(UC_ARM_REG_SP,0x3c4f0000)
            t.uc.reg_write(UC_ARM_REG_PC,entry);t.uc.reg_write(UC_ARM_REG_LR,STOP|1);t.uc.reg_write(UC_ARM_REG_R0,arg)
            w={'tid':tid,'tcb':tcb,'arg':arg,'context':t.uc.context_save(),'returned':False,'exited':False,'joined':False,'delay':0,'value':0,'joininfo':0}
            self.workers.append(w);t.uc.context_restore(main);return 0
        def pause(self,t):assert t.reg(0)==10000;t.uc.emu_stop();return 0
        def complete_exit(self,w):
            w['exited']=True
            if self.band:self.put(w['tcb']+64,0x4000)
            else:
                self.put(self.table+(w['tid']&255)*4,0)
                info=self.alloc(40);self.allocations[info]['kind']='joininfo';w['joininfo']=info
                self.put(info+8,w['tid']);self.t.uc.mem_write(info+6,b'\1');self.put(info+36,w['value'])
                # Only our outstanding join record exists; no previous owner.
                assert self.t.word(self.group+56)==0;self.put(self.group+56,info,info)
        def advance(self):
            for w in self.workers:
                if w.get('wait_sem') and w['wait_sem'] not in self.signaled:continue
                if w.get('wait_sem'):self.signaled.discard(w.pop('wait_sem'))
                self.current_worker=w
                if w['returned']:
                    if not w['exited'] and self.mode!='missing_exit':
                        w['delay']+=1
                        if w['delay']>=(25 if self.mode=='delayed_exit' else 1):self.complete_exit(w)
                    continue
                if self.mode=='unscheduled':continue
                main=self.t.uc.context_save();self.t.uc.context_restore(w['context']);self.put(self.current,w['tcb']);self.t.stopped=False
                try:self.t.uc.emu_start(self.t.uc.reg_read(UC_ARM_REG_PC)|1,0,count=100000)
                except Exception:
                    print('Worker model exception:',self.target,self.mode,'tid',w['tid'],'pc',hex(self.t.uc.reg_read(UC_ARM_REG_PC)),'lr',hex(self.t.uc.reg_read(UC_ARM_REG_LR)))
                    raise
                if self.t.stopped:
                    w['returned']=True;w['value']=self.t.reg(0)
                    if self.mode=='wrong_return':w['value']=123
                w['context']=self.t.uc.context_save();self.t.uc.context_restore(main);self.put(self.current,self.uitcb)
        def call(self,entry,args=()):
            self.t.stopped=False;self.t.uc.reg_write(UC_ARM_REG_LR,STOP|1);self.t.uc.reg_write(UC_ARM_REG_SP,0x3c4ff000)
            for r,v in zip(regs,args):self.t.uc.reg_write(r,v)
            self.t.uc.emu_start(entry|1,0,count=2000000);assert self.t.stopped,'instruction budget exhausted';return self.t.reg(0)
        def create_timer(self,t):
            if self.mode=='timer_fail':return 0
            self.callback=t.reg(0);assert t.reg(1)==100;self.runtime=t.reg(2);self.timer_live=True
            self.put(self.timer+8,self.callback,self.runtime)
            if self.band:
                self.put(self.foreign+28,self.timer);self.put(self.timer+24,self.foreign,0);self.put(0x200bd110,self.timer)
                self.allocations[self.runtime]['kind']='runtime'
            return self.timer
        def delete_timer(self,t):
            assert t.reg(0)==self.timer and self.timer_live;self.timer_live=False
            if self.band:
                self.put(self.foreign+28,0);self.put(0x200bd110,self.foreign)
                if self.mode=='foreign_reference':self.put(self.foreign+12,self.runtime)
            return 0
        def free(self,t):
            p=t.reg(0)
            if p in self.allocations:
                a=self.allocations[p];assert not a.get('freed'),('double free',hex(p));a['freed']=True;self.frees.append((p,a['kind']))
                if a['kind']=='session':assert any(w['arg']==p and w['exited'] and w['joined'] for w in self.workers),'owner freed before join'
                if a['kind']=='runtime':assert not self.timer_live and all(w['joined'] for w in self.workers)
            return 0
        def verify_join(self,t):
            w=next(w for w in self.workers if w['tid']==t.reg(0));self.join_calls+=1
            if not w['exited']:self.join_while_alive+=1
            assert w['exited'] and w['returned'] and not w['joined'],'JOIN before kernel exit proof'
            return w
        def observe_join(self,u,a,n,data):
            if a==self.a['join'] and self.mode!='join_fail':
                w=self.verify_join(self.t);w['joined']=True
        def fail_join(self,t):self.verify_join(t);return 3
        def band_join(self,t):
            w=self.verify_join(t)
            if self.mode=='join_fail':return 3
            self.put(t.reg(1),w['value']);w['joined']=True;self.put(self.table+(w['tid']&255)*4,0);return 0
        def cache(self,t):
            t.uc.mem_write(0x1c300000,bytes(t.uc.mem_read(0x3c300000,0x400000)));return 0
        def opened(self,t):return 51 if t.string(t.reg(0))=='/tmp/ns_worker_v2_result.txt' else 42
        def printed(self,t):
            if t.string(t.reg(1))=='NS2':self.row=[]
            elif t.string(t.reg(1))==' %u':self.row.append(t.reg(2))
            else:
                assert t.string(t.reg(1))=='\n' and len(self.row)==20
                self.reports.append(list(self.row))
            return 1
        def exercise(self):
            for tick in range(205):
                if not self.timer_live:break
                for _ in range(10):self.advance()
                self.put(self.current,self.uitcb);self.call(self.callback,[self.timer])
            assert not self.timer_live,'timer never stopped'
        def summary(self,row):
            assert row[0]==0x4e533252 and len(row)==20
            normal=self.mode in {'normal','delayed_exit','foreign_reference'}
            if normal:
                assert row[2:8]==[8,8,8,8,4,4],row
                assert row[8]>0 and row[9]==8 and row[12:19]==[0,0,1,0,1,0x4e533250,6],row
                assert len([1 for _,kind in self.frees if kind=='session'])==8
            else:assert row[12] or row[13],row
            assert self.join_while_alive==0
            return {'case':self.mode,'report':row,'workers':len(self.workers),'join_calls':self.join_calls,
                'join_while_alive':self.join_while_alive,'session_frees':sum(kind=='session' for _,kind in self.frees),'passed':True}

    class RadioHarness(Harness):
     def __init__(self,target,mode,t):
      super().__init__(target,mode,t)
      self.events=[];self.tokens={};self.remotes={};self.sems=set();self.signaled=set();self.current_worker=None;self.barrier_posts=0;self.destroyed_sems=0;self.dispatches=0;self.injected=False
      self.client=0x3c370000;self.regnode=0x3c371000;self.internal=0x3c372000;self.loopdata=0x3c373000;self.external=0 if mode=='internal_callbacks' else 0x3c374000
      self.reghead=0x20083274 if self.band else 0x3c21cdd4
      self.put(self.reghead,self.regnode,self.regnode);self.put(self.regnode,self.reghead,self.reghead)
      self.put(self.regnode+8,42);self.put(self.regnode+(24 if self.band else 20),self.client)
      t.uc.mem_write(self.regnode+(33 if self.band else 25),b'local\0')
      self.put(self.client+(52 if self.band else 48),self.internal,self.external)
      self.put(self.internal,self.loopdata);self.put(self.loopdata+64,self.loopdata)
      t.uc.mem_write(self.loopdata+(184 if self.band else 180),b'\1')
      self.fifo=self.loopdata+(188 if self.band else 184);self.put(self.fifo,self.fifo,self.fifo)
      self.qhead=self.client+(76 if self.band else 72);self.put(self.qhead,self.qhead,self.qhead)
      if not self.external:self.put(self.qhead,0,0) # unused external queue is not a live delivery source
      self.loop_tcb=0x20130000 if self.band else 0x3c318000
      self.put(self.loop_tcb+8,self.group);self.put(self.loop_tcb+(44 if self.band else 12),500)
      if not self.band:
       self.foreign=0x3c300800;self.put(self.foreign+8,0x2c7c7a8f,0);self.put(0x3c2c5eb4,24,self.foreign,self.foreign)
      t.stubs[self.a['rpc'] if 'rpc' in self.a else (0xc46a18c if self.band else 0x2c7d47e6)]=self.rpc
      if self.band:
       t.stubs.update({0xc3f4b60:lambda t:0,0xc3f4b7c:lambda t:0,0xc3f5668:self.wake,
        0xc3494fc:self.sem_init,0xc34daf2:self.sem_wait,0xc34f588:self.sem_post,0xc34d95a:self.sem_destroy,
        0xc721410:self.copied,0xc46cc08:self.forbid})
      else:
       t.stubs.update({0x2c483a20:lambda t:0,0x2c483a3c:lambda t:0,0x2c47ea48:self.wake,
        0x2c34ad00:self.sem_init,0x2c33a5aa:self.sem_wait,0x2c33a600:self.sem_post,0x2c33a584:self.sem_destroy,
        0x2c7cbe8c:self.forbid,0x2c7cf084:self.forbid,0x2c72a2f8:self.copied})
      if mode=='no_client':self.put(self.reghead,self.reghead,self.reghead)
      if mode=='sem_fail':t.stubs[0xc8bbb10 if self.band else 0x2c8a9d60]=lambda t:0xffffffff
     def forbid(self,t):raise AssertionError('borrowed system client must not be created/deleted')
     def copied(self,t):t.uc.mem_write(t.reg(0),bytes(t.uc.mem_read(t.reg(1),t.reg(2))));return t.reg(0)
     def create_timer(self,t):
      p=super().create_timer(t)
      if p and not self.band:
       self.put(self.foreign+28,p);self.put(p+24,self.foreign,0);self.put(0x3c2c5ebc,p)
      return p
     def delete_timer(self,t):
      v=super().delete_timer(t)
      if not self.band:self.put(self.foreign+28,0);self.put(0x3c2c5ebc,self.foreign)
      return v
     def opened(self,t):return 51 if t.string(t.reg(0))=='/tmp/ns_ble_v3_result.txt' else 42
     def printed(self,t):
      if t.string(t.reg(1))=='NS3':self.row=[]
      elif t.string(t.reg(1))==' %u':self.row.append(t.reg(2))
      else:
       assert t.string(t.reg(1))=='\n' and len(self.row)==20;self.reports.append(list(self.row))
      return 1
     def rpc(self,t):
      assert t.reg(0)==self.client
      packet,cmd=t.reg(1),t.reg(2);scan_start=0xed if self.band else 0xd1;scan_stop=0xee if self.band else 0xd3
      assert self.current_worker is not None
      w=self.current_worker;tokenoff=60 if self.band else 44
      if cmd in (scan_start,0x43):
       token=t.word(packet+tokenoff);adv=cmd==0x43;info={'token':token,'adv':adv,'worker':w,'stopped':False}
       assert token in self.allocations;self.allocations[token]['kind']='token';self.tokens[token]=info
       remote=0x47000000+len(self.tokens);self.remotes[remote]=info
       self.put(packet+(12 if self.band else 4),0 if self.mode=='zero_remote' else remote)
       if self.band:self.put(packet+16,0)
       table=t.word(token+(4 if adv or not self.band else 12));assert t.word(table)==(12 if adv else 16)
       if not adv:
        expected=b'\0\1\0\0\0' if self.band else b'\1\1'+bytes(6)
        assert bytes(t.uc.mem_read(packet+(68 if self.band else 48),len(expected)))==expected
       else:
        assert bytes(t.uc.mem_read(packet+(100 if self.band else 76),12))==b'\x02\x01\x06\x08\x09NS5-'+(b'B11' if self.band else b'S4S')
        assert t.word(packet+(84 if self.band else 64))==(320 if self.band else 160)
       if self.mode=='start_rpc_fail':return 1
       if self.mode!='start_timeout':self.events.append(('start',info,1 if self.mode=='start_fail' else 0))
      elif cmd in (scan_stop,0x44):
       remote=t.word(packet+tokenoff);info=self.remotes[remote];assert info['worker'] is w and info['adv']==(cmd==0x44)
       assert not self.allocations[info['token']].get('freed'),'no stop of a reclaimed failed-start token'
       if self.mode=='stop_rpc_fail':return 1
       if self.mode!='stop_timeout':self.events.append(('stop',info,0))
      else:raise AssertionError(hex(cmd))
      return 0
     def sem_init(self,t):
      p=t.reg(0);assert t.reg(1)==0 and t.reg(2)==0;self.sems.add(p);self.put(p,0,0,0);return 0
     def sem_wait(self,t):
      p=t.reg(0);assert p in self.sems and self.current_worker is not None
      if p not in self.signaled:self.current_worker['wait_sem']=p;t.uc.emu_stop()
      else:self.signaled.remove(p)
      return 0
     def sem_post(self,t):p=t.reg(0);assert p in self.sems;self.signaled.add(p);return 0
     def sem_destroy(self,t):
      p=t.reg(0)
      if p in self.sems:self.sems.remove(p);self.destroyed_sems+=1
      return 0
     def wake(self,t):self.barrier_posts+=1;return 0
     def dispatch_event(self,event):
      kind,info,status=event;token=info['token'];frame=self.alloc(720 if self.band else 640);self.allocations[frame]['kind']='frame'
      self.put(frame+8,self.client)
      if self.band:
       b=frame+12
       if info['adv']:
        if kind=='start':self.put(b,0x117);self.put(b+60,token);self.t.uc.mem_write(b+68,bytes([11,status]))
        else:self.put(b,0x118);self.put(b+76,token);self.t.uc.mem_write(b+84,bytes([12 if self.mode=='wrong_adv_id' else 11]))
       else:
        if kind=='start':self.put(b,0x168);self.put(b+340,token);self.put(b+348,status)
        elif kind=='stop':self.put(b,0x169);self.put(b+356,token)
        else:self.put(b,0x167);self.put(b+60,token);self.t.uc.mem_write(b+78,b'\3');self.t.uc.mem_write(b+81,b'\2\1\6')
       entry=0xc467848
      else:
       code=(0xfb if kind=='start' else 0xfc) if info['adv'] else (0x139 if kind=='start' else (0x13a if kind=='stop' else 0x138))
       self.t.uc.mem_write(frame+12,struct.pack('<H',code))
       if code==0xfb:self.put(frame+56,token);self.t.uc.mem_write(frame+60,bytes([11,status]))
       elif code==0xfc:self.put(frame+64,token);self.t.uc.mem_write(frame+68,bytes([12 if self.mode=='wrong_adv_id' else 11]))
       elif code==0x139:self.put(frame+0x148,token,status)
       elif code==0x13a:self.put(frame+0x150,token)
       else:self.put(frame+56,token);self.t.uc.mem_write(frame+60,bytes(10)+bytes([3,0]));self.t.uc.mem_write(frame+72,b'\2\1\6')
       entry=0x2c7d6a70
      if self.mode=='wrong_callback':self.put(self.current,self.loop_tcb if self.external else self.uitcb)
      else:self.put(self.current,self.uitcb if self.external else self.loop_tcb)
      self.call(entry,[frame]);self.dispatches+=1;self.put(self.current,self.uitcb)
      if kind=='start' and not status and not info['adv']:self.events.append(('report',info,0))
      if kind=='stop':info['stopped']=True
     def free(self,t):
      p=t.reg(0)
      assert p not in [getattr(self,'client',0),getattr(self,'regnode',0),getattr(self,'internal',0),getattr(self,'external',0)],'shared client identity freed'
      a=self.allocations.get(p)
      if a and a['kind']=='session':
       assert all(self.allocations[token].get('freed') for token,info in self.tokens.items() if info['worker']['arg']==p),'owner free before decoder token reclamation'
      result=super().free(t)
      if a and a['kind']=='token':self.t.uc.mem_write(p,bytes([0xdd])*a['bytes'])
      return result
     def inject_queue(self):
      if self.injected or not self.tokens:return
      if self.mode not in {'own_queue','foreign_queue','bad_queue','token_timer','client_lost'}:return
      if not any(info['stopped'] for info in self.tokens.values()):return
      self.injected=True
      token=next(iter(self.tokens));node=0x3c37e000
      if self.mode=='client_lost':self.put(self.regnode+(24 if self.band else 20),0);return
      if self.mode=='token_timer':self.put(self.foreign+12,token);return
      if self.mode=='bad_queue':self.put(self.qhead,0,0);return
      self.put(self.qhead,node,node);self.put(node,self.qhead,self.qhead);self.put(node+8,self.client)
      saved=token if self.mode=='own_queue' else 0x3c123400
      if self.band:self.put(node+12,0x169);self.put(node+12+356,saved,0)
      else:self.t.uc.mem_write(node+12,struct.pack('<H',0x13a));self.put(node+0x150,saved)
     def radio_step(self):
      while self.events:self.dispatch_event(self.events.pop(0))
      self.inject_queue()
      if self.mode!='barrier_stall' and self.t.word(self.fifo+4)!=self.fifo:
       self.put(self.current,self.loop_tcb)
       self.call(0xc4642f4 if self.band else 0x2c7c9436,[self.loopdata+64]);self.put(self.current,self.uitcb)
     def exercise(self):
      for tick in range(405):
       if not self.timer_live:break
       for _ in range(10):self.advance();self.radio_step()
       self.put(self.current,self.uitcb);self.call(self.callback,[self.timer])
      assert not self.timer_live,'timer never stopped'
     def summary(self,row):
      assert row[0]==0x4e533352 and len(row)==20
      good=self.mode in {'normal','delayed_exit','foreign_queue','foreign_reference','internal_callbacks'}
      if good:
       assert row[2:10]==[4,4,4,4,0x30003,0x30003,4,4],row
       assert row[10]>=12 and row[11]>0 and row[12:19]==[0,0,1,0,1,0x4e533350,6],row
       assert row[19]==(2 if self.mode=='internal_callbacks' else 1)
       assert len(self.tokens)==6 and all(self.allocations[p].get('freed') for p in self.tokens)
       assert self.barrier_posts==self.destroyed_sems==4 and not self.sems
      else:assert row[12] or row[13],row
      assert self.join_while_alive==0
      return {'case':self.mode,'report':row,'workers':len(self.workers),'join_calls':self.join_calls,'join_while_alive':self.join_while_alive,'session_frees':sum(k=='session' for _,k in self.frees),'token_frees':sum(k=='token' for _,k in self.frees),'barrier_posts':self.barrier_posts,'sems_destroyed':self.destroyed_sems,'passed':True}

    results={'stage':'NS6 bridge','hardware_tested':False,'real_code':['page bridge and BLE worker/backend/controller','ROM event decoders/token reclamation and FIFO','S4 actual completed pthread_join'],'models':['page lifecycle edges (native page shell not exercised here)','kernel scheduler, RPC, events, semaphores, heap and timers'],'cases':{}}
    for target in ['band11','s4']:
     rows=[]
     for mode in ['normal','delayed_exit','early_close','coalesced','create_fail','join_fail','wrong_return','missing_exit','stop_timeout','barrier_stall','own_queue','foreign_queue','token_timer','client_lost','no_client']:
      t=Machine();band=target=='band11';base=0xc0c0000 if band else 0x2c300000
      ap=(old/('band11/179-vela_ap.bin' if band else 'vela_ap.bin')).read_bytes();size=(len(ap)+4095)&~4095
      t.uc.mem_map(base,size);t.uc.mem_write(base,ap)
      if band:t.uc.mem_map(base+0x20000000,size);t.uc.mem_write(base+0x20000000,ap)
      t.uc.mem_map(0x20000000,0x200000);t.uc.mem_map(0x3c200000,0x100000);t.uc.mem_map(0x3c300000,0x400000);t.uc.mem_map(0x1c300000,0x400000)
      h=RadioHarness(target,mode,t);sym=json.loads((R/'engineering-build'/(target+'_bridge_symbols.json')).read_text())
      code=0x1c600000;raw=(R/'engineering-build'/(target+'_bridge.bin')).read_bytes();t.uc.mem_write(code,raw);t.uc.mem_write(code+0x20000000,raw)
      p=0x3c390000
      def call(name,*args):return h.call(code+(sym[name]&~1),[p,*args])
      def fields():return [call('pg_field',i) for i in range(20)]
      def tick(n=1):
       for _ in range(n):
        for _ in range(10):h.advance();h.radio_step()
        h.put(h.current,h.uitcb);call('ns5page_poll')
      if mode=='normal':
       for k in range(4):assert call('pg_begin_case',k)==3,(target,k)
       guard=h.a['thread'];saved=bytes(t.uc.mem_read(guard,4));t.uc.mem_write(guard,bytes(4));assert call('pg_begin_case',4)==3;t.uc.mem_write(guard,saved)
       assert fields()[3:6]==[0,0,0] and not h.workers
      assert call('pg_init')==0
      call('ns5page_resume');tick(1 if mode=='early_close' else 8)
      if mode in ['normal','delayed_exit','coalesced','foreign_queue']:
       before=fields();assert before[3]==1 and before[5]==0 and before[2]==2,before
       # limit=0 must keep one lease active beyond the old finite worker limit.
       tick(120);assert fields()[3:6]==[1,0,0],fields()
      call('ns5page_pause');call('ns5page_resume')
      if mode=='coalesced':
       call('ns5page_pause');call('ns5page_resume');call('ns5page_pause');call('ns5page_resume')
      tick(65)
      good=mode in ['normal','delayed_exit','early_close','coalesced','foreign_queue']
      if good:
       assert fields()[3:6]==[2,1,1],fields()
       call('ns5page_pause');tick(60);call('ns5page_resume');tick(8);call('ns5page_pause');tick(60)
       v=fields();radio=2 if mode=='early_close' else 3;assert v[:2]==[0,0] and v[2:12]==[6,3,3,3,radio,radio,radio,radio,3,3],v
       assert v[15]>=1 and v[16]==0 and v[19]==0 and call('pg_pass')==(0 if mode=='early_close' else 1),v
       assert all(h.allocations[token].get('freed') for token in h.tokens)
      else:
       tick(400);v=fields();assert v[0] or v[1] or (mode in ['missing_exit','barrier_stall','own_queue','token_timer'] and v[2] in [3,4,5]),(mode,v)
       old_created=v[3];call('ns5page_pause');call('ns5page_resume');tick(30);assert fields()[3]==old_created
      assert h.join_while_alive==0
      assert sum(not w['returned'] for w in h.workers)<=1
      rows.append({'case':mode,'fields':v,'workers':len(h.workers),'join_calls':h.join_calls,'joined_before_exit':h.join_while_alive,'passed':True})
     results['cases'][target]=rows
    ui_results=[]
    for ui_mode in ['three_cycles','event_pending','id_occupied']:
     holder={}
     def setup_ui(t,events,allocs):
      t.instruction_budget=3000000;h=RadioHarness('s4','normal',t);holder['h']=h
      h.put(0x3c2c5eb4,24,h.foreign,h.foreign)
      for head in [0x3c202710,0x3c202844,0x3c21c25c]:h.put(head,head,head)
      h.put(0x3c202794,81);h.put(0x3c202798,351)
      page=0x3c320000;h.put(0x3c2027d0+68,page);h.put(page+64,0x3c2027d0);h.put(page+20,0x20000)
      if ui_mode=='id_occupied':
       app=0x3c320100;h.put(0x3c202710,app,app);h.put(app,0x3c202710,0x3c202710);h.put(app+16,0xca)
      def printed(t):
       fmt=t.string(t.reg(1))
       if fmt=='NS6':h.row=[]
       elif fmt==' %u':h.row.append(t.reg(2))
       else:assert fmt=='\n' and len(h.row)==20;h.reports.append(list(h.row))
       return 1
      t.stubs[0x2c34c35c]=printed
     def after_ui(t,events):
      h=holder['h'];top=[0];state={};root=0x3c330000;label=root+512
      def dup(t):
       b=t.string(t.reg(0)).encode()+b'\0';p=h.alloc(len(b));t.uc.mem_write(p,b);return p
      def zero(t):t.uc.mem_write(t.reg(0),bytes(t.reg(1)));return t.reg(0)
      def memset(t):t.uc.mem_write(t.reg(0),bytes([t.reg(1)&255])*t.reg(2));return t.reg(0)
      def subscribe(t):
       subject=0x3c324000;node=subject+128;head=subject+20;h.put(head,node,node);h.put(node,head,head);h.put(node+16,0x2000a001);return subject
      def text(t):assert t.reg(0)==label and t.string(t.reg(1)).startswith('NS6 NATIVE');return 0
      t.stubs.update({0x2c47f930:lambda t:0,0x2c3491d8:dup,0x2c4dd0cc:zero,0x2c8e3cb0:memset,0x2c75ec60:lambda t:h.alloc(64),0x2c75ec78:lambda t:0,0x2c75ed24:lambda t:0,
       0x2c8c61f8:h.free,0x2c897448:subscribe,0x2000a000:lambda t:0,
       0x2c4f7044:lambda t:h.alloc(t.reg(0)),0x2c4f6b20:h.free,0x2c355d22:h.copied,
       0x2c75f2cc:lambda t:top[0],0x2c75c2b0:lambda t:int(top[0]==state.get('page')),
       0x2c4d9e34:lambda t:1,0x2c7a5fe4:lambda t:root,0x2c4d6d9a:lambda t:0,0x2c4dd7e4:lambda t:0,0x2c75cea4:lambda t:0,
       0x2c4d6e50:lambda t:label,0x2c4da4e8:lambda t:0,0x2c506a80:text,0x2c4dd3c8:lambda t:0,0x2c4d92d2:lambda t:0,
       0x2c7a4afc:lambda t:2,0x2c7a689c:lambda t:0,0x2c4d757e:lambda t:0,0x2c4dfa60:lambda t:0,
       0x2c8d2904:lambda t:0,0x2c580220:lambda t:0,0x2c4df596:lambda t:0,0x2c8e4000:lambda t:len(t.string(t.reg(0)))})
      fi=h.call(0x2c4f7536,[0x3c207640]);fn=h.alloc(16);t.uc.mem_write(fn,b'other.app\0');h.put(fi,0x1a,0x2caa6a4c,fn,0x2caad1e5)
      fo=h.call(0x2c4f7536,[0x3c207b40]);t.uc.mem_write(fo,b'other.app\0');t.uc.mem_write(fo+128,b'\1')
      foreign=[]
      for i in range(15):
       row=h.alloc(8);h.put(row,0x3c340000+i*256,0x3c350000+i*256);foreign.append(row);h.put(0x3c3e84a8+4*i,row)
      t.uc.mem_write(0x3c3e84f8,b'\x0f')
      def tick(n=1):
       for _ in range(n):
        for _ in range(10):h.advance();h.radio_step()
        h.put(h.current,h.uitcb);h.call(h.callback,[h.timer])
      tick()
      if ui_mode=='id_occupied':
       tick(10);assert h.reports[-1][12]==72;ui_results.append({'case':ui_mode,'passed':True});return
      app=h.call(0x2c796b60,[0xca]);page=h.call(0x2c75ee00,[0xca0000]);state['page']=page;assert app and page
      if ui_mode=='event_pending':
       tick(10);assert h.reports[-1][15]==0 and not h.workers;ui_results.append({'case':ui_mode,'passed':True});return
      h.call(0x2c75e43c);tick(10);assert h.reports[-1][15]==7,(h.reports[-1], [hex(t.word(a)) for a in [0x3c207640,0x3c207644,0x3c207648,0x3c207b40,0x3c207b44,0x3c207b48]],hex(app),t.string(t.word(app+8)))
      for i in range(3):
       top[0]=page;h.call(0x2c75d274,[page]);tick(60 if i else 8)
       top[0]=0;h.call(0x2c75d3d4,[page]);tick(1 if i==0 else 60)
      tick(65);v=h.reports[-1]
      assert v[2:8]==[0x30003]*6 and v[12:18]==[0,0,1,7,1,0x4e533650],v
      assert v[10]>=0 and h.timer_live and len(h.workers)==3 and h.join_while_alive==0
      assert [t.word(0x3c3e84a8+4*i) for i in range(15)]==foreign and all(not h.allocations[x].get('freed') for x in foreign)
      ui_results.append({'case':ui_mode,'report':v,'passed':True})
     loader=run_case(blob_override=(R/'engineering-build/ns_default_v6.elf').read_bytes(),setup_extra=setup_ui,after_init=after_ui,expected_unload=0xffffffff)
     assert loader['rmmod_return']==0xffffffff
    (R/'engineering-build/s4-ui-verification.json').write_text(json.dumps({'hardware_tested':False,'artifacts':{'ns_default_v6.elf':hashlib.sha256((R/'engineering-build/ns_default_v6.elf').read_bytes()).hexdigest()},'cases':ui_results,'real_code':'NS6 ELF load/refusal to unload; ROM app installation/events/launcher publication/native page lifecycle wrappers plus real bridge/BLE decoders/FIFO/join','models':'widget/navigation effects, heap, RPC/events, kernel scheduler, timers and semaphores; complete list renderer not exercised'},indent=2))
    results['artifacts']={n:hashlib.sha256((R/'engineering-build'/n).read_bytes()).hexdigest() for n in ['band_default.bin','band_default_bootstrap.bin','ns_default_v6.elf','band11_bridge.bin','s4_bridge.bin']}
    (R/'engineering-build/page-verification.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'passed':True,'band_cases':len(results['cases']['band11']),'s4_cases':len(results['cases']['s4']),'hardware_tested':False}))
if __name__=='__main__':main()
