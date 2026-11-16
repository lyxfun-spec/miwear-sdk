"""Build NS6 page-session runtimes; never installs or uses BLE."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,sys
R=Path(__file__).resolve().parents[1]
from build_context import require_build_tree
require_build_tree(R)
def sha(b):return hashlib.sha256(b).hexdigest()
def run(args):subprocess.run([str(x) for x in args],check=True,cwd=R)
def normalize_local_rel32(blob):
 # The frozen PIC archive uses local PC-relative literal pointers. Once all
 # code/rodata is merged into the same .text, S+A-P is independent of the
 # eventual load base. Resolve ONLY those same-section references offline;
 # keep every cross-section relocation for the firmware loader unchanged.
 from io import BytesIO
 from elftools.elf.elffile import ELFFile
 e=ELFFile(BytesIO(blob));assert e.elfclass==32 and e.little_endian
 result=bytearray(blob);proof=[]
 for index,section in enumerate(e.iter_sections()):
  if section['sh_type']!='SHT_REL':continue
  target=e.get_section(section['sh_info']);symbols=e.get_section(section['sh_link']);kept=[]
  for rel in section.iter_relocations():
   if rel['r_info_type']!=3:kept.append(struct.pack('<II',rel['r_offset'],rel['r_info']));continue
   sym=symbols.get_symbol(rel['r_info_sym']);offset=rel['r_offset']
   assert sym['st_shndx']==section['sh_info'] and target.name=='.text' and offset%4==0 and offset+4<=target['sh_size']
   at=target['sh_offset']+offset;addend=struct.unpack_from('<I',result,at)[0]
   value=(sym['st_value']+addend-offset)&0xffffffff;struct.pack_into('<I',result,at,value)
   proof.append({'offset':offset,'symbol':sym.name,'symbol_value':sym['st_value'],'addend':addend,'resolved':value,'same_section':target.name})
  data=b''.join(kept);result[section['sh_offset']:section['sh_offset']+len(data)]=data
  struct.pack_into('<I',result,e['e_shoff']+index*e['e_shentsize']+20,len(data))
 return result,proof

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--zig',type=Path,required=True);a.add_argument('--research',type=Path,required=True);args=a.parse_args()
 old=args.research.resolve();sys.path.insert(0,str(old/'python-deps'));from elf_utils import inspect_elf
 zig=args.zig.resolve();assert subprocess.check_output([str(zig),'version'],text=True).strip()=='0.13.0'
 out=R/'engineering-build';out.mkdir(exist_ok=True)
 core_addresses={
 'band11':[0xc3514ec,0xc34cc7c,0xc348784,0xc3487b6,0xc350c54,0xc359764,0xc34cb8c,0xc3abc88,0xc3abdd8,0xc354c4c,0xc92e6da,0xc91ca02,0xc0c181e],
 's4':[0x2c35a16c,0x2c358260,0x2c34aacc,0x2c34aafe,0x2c351b7c,0x2c336940,0x2c356348,0x2c4f7488,0x2c4f6f78,0x2c334b00,0x2c334b0e,0x2c334b18,0x2c334b26,0x2c334b3c,0x2c334b40,0x2c334c36,0x2c334c3e,0x2c3369a6,0x2c336ac2,0x2c336ac8,0x2c3356f2,0x2c6a74c4,0x2c34c35c,0x2c6a6c2c]}
 radio_addresses={
 'band11':[0xc46a18c,0xc465cd4,0xc4642f4,0xc464154,0xc8bbb10,0xc3f513c,0xc3f5104,0xc467848,0xc463f7a,0xc463bd4,0xc46b514,0xc46b602,0xc46ce54],
 's4':[0x2c7d47e6,0x2c7cbd04,0x2c7c9436,0x2c7c93d4,0x2c8a9d60,0x2c485930,0x2c48588c,0x2c7d6a70,0x2c886420,0x2c886460,0x2c483a20,0x2c483a3c]}
 core_addresses['s4'] += [0x2c796b60,0x2c79693c,0x2c75ee00,0x2c7a23cc,0x2c75c2b0,0x2c4fe910,0x2c506a80,0x2c4dd3c8,0x2c4d9e34,0x2c5862ac]
 flags=['-target','thumb-freestanding-eabi','-mfloat-abi=soft','-Oz','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-Wall','-Wextra','-Werror','-I',R/'include','-I',out,'-I',R/'platform']
 sources=['src/ns_default_shell.c']
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
  sdk_manifest=json.loads((R/'sdk-export/manifest.json').read_text())
  library=R/'sdk-export/lib'/('libns_session_sdk_'+target+'.a')
  assert sha(library.read_bytes())==sdk_manifest['files']['lib/'+library.name]
  if target=='band11':
   objs=[]
   for source in sources+['platform/band_default.c']:
    obj=out/(Path(source).stem+'.o');run([zig,'cc',*flags,define,'-mcpu='+cpu,'-fPIC','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-c',R/source,'-o',obj]);objs.append(obj)
   elf=out/'band_default.elf';run([zig,'ld.lld','-T',R/'device/sdk_runtime.ld',*objs,library,'-o',elf])
   image,symbols=inspect_elf(elf,True);image+=bytes.fromhex('00bf')*((-len(image)%4)//2);assert len(image)%4==0 and len(image)<=28672
   (out/'band_default.bin').write_bytes(image);tick=symbols['owned_callback']&~1
   assert tick==0
   fnv=2166136261
   for b in image:fnv=((fnv^b)*16777619)&0xffffffff
   names={'CREATE':'page_create','EDGE':'page_edge','DESTROY':'page_destroy','RESUME':'page_resume','PAUSE':'page_pause','TITLE':'title_provider','SIGNAL':'app_signal','PAGESIGNAL':'page_signal'}
   (out/'default_image.h').write_text(f'#define NS6_IMAGE_SIZE {len(image)}u\n#define NS6_IMAGE_HASH {fnv}u\n'+''.join(f'#define NS6_{k} {symbols[v]&~1}u\n' for k,v in names.items()),encoding='ascii')
   asm=''' .syntax unified
.arch armv8-m.main
.thumb
.section .text.copy_image,"ax",%progbits
.global copy_runtime_image
.type copy_runtime_image,%function
.thumb_func
copy_runtime_image:
 adr.w r1,.Limage
 add.w r1,r1,#0x20000000
 movw r2,#@SIZE@
.Lcopy:
 ldr r3,[r1],#4
 str r3,[r0],#4
 subs r2,#4
 bne .Lcopy
 bx lr
.balign 4
.Limage:
 .incbin "@FILE@"
.size copy_runtime_image,.-copy_runtime_image
'''.replace('@SIZE@',str(len(image))).replace('@FILE@',(out/'band_default.bin').as_posix())
   (out/'copy_page.S').write_text(asm,encoding='ascii');boots=[]
   for source in [R/'device/band_worker_entry.S',R/'device/band_default_bootstrap.c',out/'copy_page.S']:
    obj=out/(source.stem+'.o');run([zig,'cc',*flags,define,'-mcpu='+cpu,'-fPIC','-fvisibility=hidden','-c',source,'-o',obj]);boots.append(obj)
   elf=out/'band_bootstrap.elf';run([zig,'ld.lld','-T',R/'device/band.ld',*boots,'-o',elf])
   boot,_=inspect_elf(elf,True);assert boot[:8]==bytes.fromhex('00f002b800000000') and boot.count(image)==1
   (out/'band_default_bootstrap.bin').write_bytes(boot)
   records[target]={'runtime_bytes':len(image),'runtime_sha256':sha(image),'symbols':symbols,'allocated_runtime_bytes':32768,'bootstrap_bytes':len(boot),'bootstrap_sha256':sha(boot)}
  else:
   unit=out/'s4_page_unit.c';unit.write_text('\n'.join('#include "'+(R/p).as_posix()+'"' for p in sources+['platform/s4_default.c'])+'\n',encoding='utf-8')
   uiobj=out/'s4_ui.o';run([zig,'cc',*flags,define,'-mcpu='+cpu,'-c',unit,'-o',uiobj])
   elf=out/'ns_default_v6.elf';run([zig,'ld.lld','-r','-T',R/'device/default_module.ld',uiobj,'--whole-archive',library,'--no-whole-archive','-o',elf])
   _,symbols=inspect_elf(elf);entry=symbols['module_initialize'];assert entry&1
   (out/'ns_default_v6.preload.elf').write_bytes(elf.read_bytes())
   blob,local_proof=normalize_local_rel32(elf.read_bytes());struct.pack_into('<I',blob,24,entry);elf.write_bytes(blob)
   records[target]={'module_bytes':len(blob),'module_sha256':sha(blob),'entry':entry,'resolved_local_rel32':local_proof}
  model_objects=[]
  for source in sources+['tests/default_page_driver.c']:
   obj=out/(target+'_model_'+Path(source).stem+'.o');run([zig,'cc',*flags,define,'-mcpu='+cpu,'-fPIC','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-c',R/source,'-o',obj]);model_objects.append(obj)
  model=out/(target+'_bridge.elf');run([zig,'ld.lld','-T',R/'device/page_model.ld',*model_objects,library,'-o',model])
  raw,model_symbols=inspect_elf(model,True);(out/(target+'_bridge.bin')).write_bytes(raw)
  (out/(target+'_bridge_symbols.json')).write_text(json.dumps(model_symbols,indent=2))
  records[target]['candidate_sdk_library_sha256']=sha(library.read_bytes())
  records[target]['ap_sha256']=expected;records[target]['rom_guards']=guards
 record={'stage':'NS6 default policy','hardware_tested':False,'targets':records,'sources':{p.relative_to(R).as_posix():sha(p.read_bytes()) for folder in ['src','include','platform','device','tools','templates','tests'] for p in (R/folder).glob('*') if p.is_file()}}
 (out/'build.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({t:{k:v for k,v in d.items() if k not in {'symbols','rom_guards','resolved_local_rel32'}} for t,d in records.items()},indent=2))
if __name__=='__main__':main()
