"""Exact-AP guarded G1 build. Does not communicate with a device."""
import argparse, hashlib, json, struct, subprocess, sys, os
from pathlib import Path
R=Path(__file__).resolve().parents[1]
from build_context import require_build_tree
require_build_tree(R)
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--research', type=Path, required=True);p.add_argument('--zig',type=Path,required=True)
 a=p.parse_args();sys.path.insert(0,str(a.research/'python-deps'))
 from elftools.elf.elffile import ELFFile
 out=R/'build';out.mkdir(exist_ok=True)
 ap=(a.research/'band11/179-vela_ap.bin').read_bytes()
 assert sha(ap)=='307adabe0ff61c77be73d045a8260e279cddd131c7a535a2678eb34199278a2b'
 zig=a.zig.resolve();assert subprocess.check_output([str(zig),'version'],text=True).strip()=='0.13.0'
 cache=Path(os.environ['ZIG_LOCAL_CACHE_DIR']).parent;cache.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,ZIG_LOCAL_CACHE_DIR=str(cache/'local'),ZIG_GLOBAL_CACHE_DIR=str(cache/'global'))
 def run(xs):subprocess.run([str(x) for x in xs],cwd=R,env=env,check=True)
 def inspect(path):
  with path.open('rb') as f:
   e=ELFFile(f);s=e.get_section_by_name('.symtab')
   assert not any(x.name and x['st_shndx']=='SHN_UNDEF' for x in s.iter_symbols())
   assert not any(x['sh_size'] and ((x['sh_flags']&2 and x.name!='.text') or x['sh_type'] in ['SHT_REL','SHT_RELA']) for x in e.iter_sections())
   return e.get_section_by_name('.text').data(),{x.name:x['st_value'] for x in s.iter_symbols() if x.name}
 core=[0xc3514ec,0xc34cc7c,0xc348784,0xc3487b6,0xc350c54,0xc359764,0xc34cb8c,0xc3abc88,0xc3abdd8,0xc354c4c,0xc92e6da,0xc91ca02,0xc0c181e]
 radio=[0xc3f5120,0xc3f455c,0xc3f5668,0xc46a18c,0xc465cd4,0xc4642f4,0xc464154,0xc8bbb10,0xc3f513c,0xc3f5104,0xc467848,0xc463f7a,0xc463bd4,0xc46b514,0xc46b602,0xc46ce54]
 windows={
  'uv_async_init':(0xc3f455c,0xc3f45f8),
  'uv_async_send':(0xc3f5668,0xc3f56e8),
  'worker_semaphore':(0xc3f5104,0xc3f5168),
  'worker_semaphore_init':(0xc8bbb10,0xc8bbb28),
  'attribute_service':(0xc4704e0,0xc470554),
  'attribute_cache_lookup':(0xc4700a8,0xc4700e0),
  'attribute_socket_branch':(0xc46e550,0xc46e56c),
  'profile_create_fence':(0xc475cc8,0xc475d9c),
  'profile_delete':(0xc473eb4,0xc473f22),
  'profile_message_lifetime':(0xc473930,0xc473c90),
  'socket_gatt_lifetime':(0xc46e482,0xc46e50c),
  'client_rpc_lifetime':(0xc46a18c,0xc46a224),
  'client_recv_lifetime':(0xc46a090,0xc46a14a),
  'scan_decoder_retirement':(0xc463f7a,0xc464058),
  'gattc_decoder':(0xc46576c,0xc465942),
  'gattc_wrappers':(0xc46ab34,0xc46af68),
  'gatts_wrappers':(0xc46af80,0xc46b300),
  'callback_router':(0xc467848,0xc4678a8),
  'stack_payload_update':(0xc87e390,0xc87e3c2),
  'stack_client_payload_dispatch':(0xc874b08,0xc874b30),
  'sal_client_callback_copy':(0xc4722fa,0xc472312),
  'sal_client_payload_event':(0xc47185a,0xc47188e),
  'service_payload_dispatch':(0xc473bd2,0xc473be4),
  'socket_payload_writer':(0xc46bf3c,0xc46bf80),
 }
 # Guard every aligned word of new GATT paths, rather than function prefixes.
 for lo,hi in windows.values():radio.extend(range(lo&~3,(hi+3)&~3,4))
 # Immutable ROM data alias contains the actual dispatch table.
 radio.extend(range(0x2ca2b724,0x2ca2b7cc,4))
 radio.extend(range(0x2ca2c134,0x2ca2c17c,4))
 radio.extend(range(0x2cc4f100,0x2cc4f128,4))
 radio.extend([0xc472350,0xc472354])
 guards={}
 for name,addresses,fn,macro in [('guards_band11.h',core,'port_check_guards','P_RD'),('radio_guards_band11.h',radio,'radio_check_guards','R_RD')]:
  pairs=[]
  for addr in sorted({b for x in addresses for b in [x&~3,(x&~3)+4]}):
   base=0x2c0c0000 if addr>=0x2c000000 else 0xc0c0000
   pairs.append((addr,struct.unpack_from('<I',ap,addr-base)[0]))
  # Identical exact-word checks in a compact readonly table, avoiding a
  # long comparison instruction sequence as the verified surface grows.
  table='static const ns_u32 '+fn+'_words[][2]={\n'+''.join(f'{{0x{x:x}u,0x{v:x}u}},\n' for x,v in pairs)+'};\n'
  loop='static ns_u32 '+fn+'(void){for(ns_u32 i=0;i<'+str(len(pairs))+'u;i++)if('+macro+'('+fn+'_words[i][0])!='+fn+'_words[i][1])return 0;return 1;}\n'
  (out/name).write_text(table+loop)

  guards[name]=pairs
 flags=['-target','thumb-freestanding-eabi','-mcpu=cortex_m33','-mfloat-abi=soft','-Oz','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-Wall','-Wextra','-Werror','-fPIC','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-DNS_TARGET_BAND=1','-I',R/'public','-I',R/'include','-I',out]
 sources=['src/g1_runtime.c','src/ns3_runtime.c','src/ns_ble_packet.c','src/freestanding.c','platform/port.c','platform/radio_port.c','platform/gatt_resource.c']
 objects=[]
 for src in sources:
  obj=out/(Path(src).stem+'.o');run([zig,'cc',*flags,'-c',R/src,'-o',obj]);objects.append(obj)
 libdir=R/'sdk-export/lib';libdir.mkdir(parents=True,exist_ok=True)
 library=libdir/'libns_link_v3_band11_candidate.a'
 if library.exists():library.unlink()
 run([zig,'ar','rcs',library,*objects])
 public=R/'sdk-export/include';public.mkdir(parents=True,exist_ok=True)
 (public/'ns_link_v3.h').write_bytes((R/'public/ns_link_v3.h').read_bytes())
 consumer=out/'g2_consumer.o';run([zig,'cc',*flags,'-c',R/'src/g2_consumer.c','-o',consumer])
 elf=out/'g1.elf';run([zig,'ld.lld','-T',R/'device/sdk_runtime.ld',consumer,library,'-o',elf])
 image,symbols=inspect(elf);image+=b'\x00\xbf'*((-len(image)%4)//2);assert len(image)<=30720 and symbols['owned_callback']==1
 (out/'g1.bin').write_bytes(image);(out/'symbols.json').write_text(json.dumps(symbols,indent=2))
 (out/'runtime_image.h').write_text('#define G1_TICK 0u\n')
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
'''.replace('@SIZE@',str(len(image))).replace('@FILE@',(out/'g1.bin').as_posix())
 # Zig's assembler cache does not track .incbin content as a source input.
 # Put the complete image digest into the assembly cache key.
 (out/'copy.S').write_text('// image SHA256 '+sha(image)+'\n'+asm)
 boots=[]
 for src in [R/'device/band_worker_entry.S',R/'device/bootstrap.c',out/'copy.S']:
  obj=out/(src.stem+'.o');run([zig,'cc',*flags,'-c',src,'-o',obj]);boots.append(obj)
 elf=out/'bootstrap.elf';run([zig,'ld.lld','-T',R/'device/band.ld',*boots,'-o',elf])
 boot,_=inspect(elf);assert boot[:8]==bytes.fromhex('00f002b800000000') and boot.count(image)==1
 (out/'bootstrap.bin').write_bytes(boot)
 layout=out/'layout.o';run([zig,'cc',*flags,'-c',R/'tests/layout.c','-o',layout])
 reslayout=out/'resource_layout.o';run([zig,'cc',*flags,'-c',R/'tests/resource_layout.c','-o',reslayout])
 model=out/'model.elf';run([zig,'ld.lld','-T',R/'device/sdk_runtime.ld',consumer,library,layout,reslayout,'-o',model])
 model_raw,model_syms=inspect(model);(out/'model.bin').write_bytes(model_raw);(out/'model-symbols.json').write_text(json.dumps(model_syms,indent=2))
 record=dict(stage='G4R2 ABI3 cancellation fix; hardware pending',hardware_tested=False,ap_sha256=sha(ap),compiler='Zig 0.13.0',runtime_bytes=len(image),bootstrap_bytes=len(boot),sdk_library_sha256=sha(library.read_bytes()),consumer_links_static_archive=True,runtime_sha256=sha(image),bootstrap_sha256=sha(boot),guards=guards,windows={n:dict(start=lo,end_exclusive=hi,sha256=sha(ap[lo-0xc0c0000:hi-0xc0c0000])) for n,(lo,hi) in windows.items()},sources={str(p.relative_to(R)).replace('\\','/'):sha(p.read_bytes()) for folder in ['public','include','src','platform','device','tools','peer','reference','vendor','tests'] for p in (R/folder).glob('*') if p.is_file()})
 (out/'build.json').write_text(json.dumps(record,indent=2)+'\n')
 print(json.dumps({k:v for k,v in record.items() if k not in ['guards','sources','windows']},indent=2))
if __name__=='__main__':main()
