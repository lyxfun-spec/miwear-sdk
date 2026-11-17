"""Build ABI1 archives and link the platform component; no device operations."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,sys,shutil
R=Path(__file__).resolve().parents[1]
from build_context import require_build_tree
require_build_tree(R)
def sha(b):return hashlib.sha256(b).hexdigest()
def run(args):subprocess.run([str(x) for x in args],check=True,cwd=R)
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--zig',type=Path,required=True);a.add_argument('--research',type=Path,required=True);args=a.parse_args()
 old=args.research.resolve();sys.path.insert(0,str(old/'python-deps'));from elf_utils import inspect_elf
 zig=args.zig.resolve();assert subprocess.check_output([str(zig),'version'],text=True).strip()=='0.13.0'
 out=R/'sdk-build';out.mkdir(exist_ok=True)
 public=out/'public-include';public.mkdir(exist_ok=True)
 for name in ['ns_session.h','ns_controller.h','ns_sdk_flow.h']:
  shutil.copyfile(R/'include'/name,public/name)
 core_addresses={
 'band11':[0xc3514ec,0xc34cc7c,0xc348784,0xc3487b6,0xc350c54,0xc359764,0xc34cb8c,0xc3abc88,0xc3abdd8,0xc354c4c,0xc92e6da,0xc91ca02,0xc0c181e],
 's4':[0x2c35a16c,0x2c358260,0x2c34aacc,0x2c34aafe,0x2c351b7c,0x2c336940,0x2c356348,0x2c4f7488,0x2c4f6f78,0x2c334b00,0x2c334b0e,0x2c334b18,0x2c334b26,0x2c334b3c,0x2c334b40,0x2c334c36,0x2c334c3e,0x2c3369a6,0x2c336ac2,0x2c336ac8,0x2c3356f2,0x2c6a74c4,0x2c34c35c,0x2c6a6c2c]}
 radio_addresses={
 'band11':[0xc46a18c,0xc465cd4,0xc4642f4,0xc464154,0xc8bbb10,0xc3f513c,0xc3f5104,0xc467848,0xc463f7a,0xc463bd4,0xc46b514,0xc46b602,0xc46ce54],
 's4':[0x2c7d47e6,0x2c7cbd04,0x2c7c9436,0x2c7c93d4,0x2c8a9d60,0x2c485930,0x2c48588c,0x2c7d6a70,0x2c886420,0x2c886460,0x2c483a20,0x2c483a3c]}
 core_addresses['s4'] += [0x2c796b60,0x2c79693c,0x2c75ee00,0x2c7a23cc,0x2c75c2b0,0x2c4fe910,0x2c506a80,0x2c4dd3c8,0x2c4d9e34,0x2c5862ac]
 flags=['-target','thumb-freestanding-eabi','-mfloat-abi=soft','-Oz','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-Wall','-Wextra','-Werror','-I',R/'include','-I',R/'private','-I',out,'-I',R/'platform']
 sources=['src/ns_session.c','src/ns_controller.c','src/ns_ble_packet.c','src/freestanding.c','src/ns_sdk_ble_backend.c','src/ns_sdk_flow.c','src/ns_sdk_page.c','platform/port.c','platform/radio_port.c']
 records={}
 for target,cpu in [('band11','cortex_m33'),('s4','cortex_m4')]:
  profile=json.loads((R/'reference'/f'{target}-abi-profile.json').read_text(encoding='utf-8'))
  ap=(old/('band11/179-vela_ap.bin' if target=='band11' else 'vela_ap.bin')).read_bytes()
  expected=profile['ap']['sha256'] if target=='band11' else profile['firmware_sha256'];assert sha(ap)==expected
  base=0xc0c0000 if target=='band11' else 0x2c300000;guards={}
  for radio,addresses in [(False,core_addresses[target]),(True,radio_addresses[target])]:
   pairs=[(addr,struct.unpack_from('<I',ap,addr-base)[0]) for addr in sorted({p for a in addresses for p in [a&~3,(a&~3)+4]})]
   name=('radio_guards_' if radio else 'guards_')+target+'.h';fn='radio_check_guards' if radio else 'port_check_guards';macro='R_RD' if radio else 'P_RD'
   (out/name).write_text('static ns_u32 '+fn+'(void){\n'+''.join(f'if({macro}(0x{addr:x}u)!=0x{value:x}u)return 0;\n' for addr,value in pairs)+'return 1;}\n',encoding='ascii')
   guards['radio' if radio else 'core']=pairs
  define='-DNS_TARGET_BAND='+('1' if target=='band11' else '0')
  objects=[]
  for source in sources:
   obj=out/(target+'_model_'+Path(source).stem+'.o')
   run([zig,'cc',*flags,define,'-mcpu='+cpu,'-fPIC','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-c',R/source,'-o',obj]);objects.append(obj)
  library=out/('libns_session_sdk_'+target+'.a')
  if library.exists():library.unlink()
  run([zig,'ar','rcs',library,*objects])
  component=out/(target+'_component.o')
  run([zig,'cc',*flags,define,'-mcpu='+cpu,'-fPIC','-fvisibility=hidden','-I',R/'templates','-c',R/'templates/page_component.c','-o',component])
  linked=out/(target+'_component.elf');run([zig,'ld.lld','-T',R/'device/sdk_component.ld',component,library,'-o',linked])
  inspect_elf(linked,True)
  example=out/(target+'_public.o')
  run([zig,'cc','-target','thumb-freestanding-eabi','-mcpu='+cpu,'-mfloat-abi=soft','-Oz','-ffreestanding','-fno-builtin','-fPIC','-Wall','-Wextra','-Werror','-I',public,'-c',R/'examples/lifecycle.c','-o',example])
  example_linked=out/(target+'_public.elf')
  run([zig,'ld.lld','-T',R/'device/sdk_component.ld','-e','example_begin',example,library,'-o',example_linked])
  inspect_elf(example_linked,True)
  records[target]=dict(library_sha256=sha(library.read_bytes()),ap_sha256=expected,example_linked=True,hardware_tested=False)
 (out/'libraries.json').write_text(json.dumps(records,indent=2)+'\n')
 print(json.dumps(records,indent=2))
if __name__=='__main__':main()
