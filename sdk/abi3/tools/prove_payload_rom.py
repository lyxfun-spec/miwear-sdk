from pathlib import Path
import sys,struct,json,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument('--research',type=Path,required=True);args=p.parse_args()
R=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(args.research/'python-deps'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
ap=(args.research/'band11/179-vela_ap.bin').read_bytes();assert hashlib.sha256(ap).hexdigest()=='307adabe0ff61c77be73d045a8260e279cddd131c7a535a2678eb34199278a2b'
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3];rows=[]
for att in [23,517]:
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 for base,size in [(0xc0c0000,0xc00000),(0x2c0c0000,0xc00000),(0x20080000,0x60000),(0x3c500000,0x100000)]:u.mem_map(base,size)
 u.mem_write(0xc0c0000,ap);u.mem_write(0x2c0c0000,ap)
 def put(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*v))
 def word(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def ret(v=0):u.reg_write(UC_ARM_REG_R0,v);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def reg(n):return u.reg_read(regs[n])
 captured={};cursor=[0x3c540000];stop=[0]
 def hook(uc,pc,size,data):
  if pc==stop[0]:u.emu_stop();return
  if pc==0xc721410:
   u.mem_write(reg(0),bytes(u.mem_read(reg(1),reg(2))));ret(reg(0))
  elif pc==0xc720f08:
   u.mem_write(reg(0),bytes([reg(1)&255])*reg(2));ret(reg(0))
  elif pc==0xc3514ec:
   a=cursor[0];cursor[0]+=(reg(0)+7)&~7;u.mem_write(a,bytes(reg(0)));ret(a)
  elif pc==0xc874b84:ret(0) # Channel-state machine does not alter payload in this fixture.
  elif pc==0xc9038e4:captured['msg']=reg(0);captured['msg_hex']=bytes(u.mem_read(reg(0),16)).hex();ret(0) # queue scheduling
  elif pc==0xc46b888:
   captured['packet']=bytes(u.mem_read(reg(1),708));captured['code']=reg(2);ret(0) # socket I/O
 u.hook_add(UC_HOOK_CODE,hook)
 # Runtime initial callback table is a modeled initialized-data fixture;
 # all values are the exact 40-byte initial table found in this AP.
 initial=ap[0xcc4f100-0xc0c0000:0xcc4f128-0xc0c0000]
 assert struct.unpack_from('<I',initial,36)[0]==0xc47185b
 u.mem_write(0x20083c2c,initial)
 def run(pc,end):
  stop[0]=end;u.reg_write(UC_ARM_REG_SP,0x3c5ff000)
  try:u.emu_start(pc|1,0,count=200000)
  except Exception:
   print('fault',hex(u.reg_read(UC_ARM_REG_PC)),hex(u.reg_read(UC_ARM_REG_LR)));raise
  assert u.reg_read(UC_ARM_REG_PC)==end
 # Actual SAL connection path copies callbacks from init table to stack global.
 run(0xc4722fe,0xc472308);assert word(0x200bd900+0x144)==0xc47185b
 event=0x3c510000;channel=0x3c511000;addr=0x3c512000
 u.mem_write(event+8,struct.pack('<H',att));u.mem_write(addr,b'ABCDEF')
 for r,v in [(UC_ARM_REG_R4,event),(UC_ARM_REG_R5,channel),(UC_ARM_REG_R6,0),(UC_ARM_REG_R8,addr),(UC_ARM_REG_R7,0x3c5fe000)]:u.reg_write(r,v)
 # Actual ATT update subtracts3, stores channel+6, then enters actual client
 # callback -> actual SAL status translation -> actual event constructor.
 run(0xc87e3a4,0xc87e3c2)
 msg=captured['msg'];assert word(msg+12)==att-3 and u.mem_read(msg,1)==b'\7' and u.mem_read(msg+8,1)==b'\0'
 connection=0x3c513000;remote=0x3c514000;callbacks=0x3c515000
 put(connection,remote);put(connection+0x38,callbacks);put(callbacks+32,0xc46bf3d);put(remote,0x3c516000);put(remote+8,0x3c517000)
 for r,v in [(UC_ARM_REG_R0,connection),(UC_ARM_REG_R5,connection),(UC_ARM_REG_R4,msg)]:u.reg_write(r,v)
 # Actual service callback branch -> actual socket callback writer.
 run(0xc473bd2,0xc4739bc)
 packet=captured['packet'];assert captured['code']==0x124 and struct.unpack_from('<I',packet,68)[0]==att-3 and packet[72]==0
 rows.append(dict(att_input=att,channel_payload=struct.unpack('<H',u.mem_read(channel+6,2))[0],event_value=word(msg+12),rpc_callback_value=struct.unpack_from('<I',packet,68)[0],status=packet[72],callback_code=hex(captured['code'])))
print(json.dumps(rows,indent=2))
(R/'build/payload-rom-proof.json').write_text(json.dumps(dict(ap_sha256=hashlib.sha256(ap).hexdigest(),hardware_tested=False,actual='Exact ROM: SAL callback copy; ATT update subtract3; stack dispatch; SAL status conversion/event constructor; service callback; RPC callback writer',modeled='Initialized RAM callback table from AP bytes, channel state-transition stub, heap allocator, queue scheduling, socket I/O, caller stack/register frames',rows=rows),indent=2)+'\n')
