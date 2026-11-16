"""Exact-AP socket/service/cache lookup. RAM cache is a synthetic sparse fixture."""
from pathlib import Path
import sys,struct,json,hashlib,argparse

class AttributeRom:
 def __init__(self,research):
  sys.path.insert(0,str(Path(research)/'python-deps'))
  from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
  from unicorn import arm_const as ar
  self.ar=ar;self.u=u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
  ap=(Path(research)/'band11/179-vela_ap.bin').read_bytes()
  assert hashlib.sha256(ap).hexdigest()=='307adabe0ff61c77be73d045a8260e279cddd131c7a535a2678eb34199278a2b'
  for base,size in [(0xc0c0000,0xc00000),(0x2c0c0000,0xc00000),(0x20080000,0x60000),(0x3c500000,0x100000)]:u.mem_map(base,size)
  u.mem_write(0xc0c0000,ap);u.mem_write(0x2c0c0000,ap)
  self.packet=0x3c510000;self.conn=0x3c511000
  def put(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*v))
  self.put=put
  # Model initialized profile registry; actual getter returns actual ROM vtable.
  put(0x200bdc8c+9*8,0x3c512000);put(0x3c512000+0x34,0xc46fdf5)
  self.enabled=0x200be6f8;u.mem_write(self.enabled,b'\1')
  # Circular list headers/node/data are modeled. Traversal/comparison are ROM.
  put(self.enabled+8,0x3c513000)
  put(0x3c513000,0,0x3c513020);put(0x3c513020,0,0x3c513000,self.conn)
  put(self.conn+0x3c,0x3c514000)
  put(0x3c514000,0,0x3c514020);put(0x3c514020,0,0x3c514000,0x3c515000)
  # Service range10..14; only characteristic12 in element array. The fixture
  # does not assert those are the physical Windows handles of any actual run.
  put(0x3c515000+4,10|(14<<16),1,0x3c516000)
  u.mem_write(0x3c516000,struct.pack('<H',12))
  u.mem_write(0x3c516004,b'\x10\0\0\0'+bytes.fromhex('91d10002d0b24cf69136f16db55b6e11'))
  u.mem_write(0x3c516018,b'\3\0\x1a\0')
  def hook(uc,pc,size,data):
   if pc==0xc46d3e6:u.emu_stop()
   elif pc==0xc721410:
    d=u.reg_read(ar.UC_ARM_REG_R0);s=u.reg_read(ar.UC_ARM_REG_R1);n=u.reg_read(ar.UC_ARM_REG_R2)
    u.mem_write(d,bytes(u.mem_read(s,n)));u.reg_write(ar.UC_ARM_REG_PC,u.reg_read(ar.UC_ARM_REG_LR))
  u.hook_add(UC_HOOK_CODE,hook)
 def query(self,h,enabled=True,valid_connection=True):
  u=self.u;a=self.ar
  u.mem_write(self.enabled,bytes([int(enabled)]));u.mem_write(self.packet,bytes(708))
  self.put(self.packet+60,self.conn if valid_connection else self.conn+4,0)
  u.mem_write(self.packet+68,struct.pack('<H',h))
  u.reg_write(a.UC_ARM_REG_R4,self.packet);u.reg_write(a.UC_ARM_REG_SP,0x3c5ff000)
  u.emu_start(0xc46e551,0,count=200000)
  assert u.reg_read(a.UC_ARM_REG_PC)==0xc46d3e6
  return u.mem_read(self.packet+12,1)[0],bytes(u.mem_read(self.packet+20,28))

def main():
 p=argparse.ArgumentParser();p.add_argument('--research',type=Path,required=True);a=p.parse_args()
 m=AttributeRom(a.research);rows=[]
 for h,enabled,valid,want in [(10,True,True,51),(11,True,True,51),(12,True,True,0),(14,True,True,51),(12,False,True,2),(12,True,False,7)]:
  status,desc=m.query(h,enabled,valid);assert status==want,(h,status,want)
  if not status:assert desc[0]==3 and struct.unpack_from('<H',desc,2)[0]==12 and struct.unpack_from('<I',desc,24)[0]==0x1a
  rows.append(dict(handle=h,enabled=enabled,valid_connection=valid,status=status,descriptor_hex=desc.hex()))
 r=dict(hardware_tested=False,actual='Exact AP socket opcode0x79 branch, profile getter/vtable, service validity, circular list traversal, interval/element lookup, descriptor copy, status write',modeled='Initialized registry, enabled/connection/cache RAM, sparse service10..14 with only element12; memcpy primitive; no physical handle or failed R3 status inferred',rows=rows)
 out=Path(__file__).resolve().parents[1]/'build/attribute-rom-proof.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(r,indent=2)+'\n')
 print('Exact ROM attribute lookup: 6 PASS; cache miss51, disabled2, invalid connection7')
if __name__=='__main__':main()
