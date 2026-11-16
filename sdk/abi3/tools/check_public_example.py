"""Compile and resolve the address-free application example against the archive."""
from pathlib import Path
import argparse,subprocess,json,hashlib,os,sys
p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--research',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build';z=a.zig.resolve()
env=dict(os.environ,ZIG_LOCAL_CACHE_DIR=os.environ['ZIG_LOCAL_CACHE_DIR'],ZIG_GLOBAL_CACHE_DIR=os.environ['ZIG_GLOBAL_CACHE_DIR'])
src=r/'examples/fixed_channel.c';obj=out/'public-example.o';linked=out/'public-example-linked.o'
subprocess.run([str(z),'cc','-target','thumb-freestanding-eabi','-mcpu=cortex_m33','-mfloat-abi=soft','-Oz','-ffreestanding','-fno-builtin','-fPIC','-Wall','-Wextra','-Werror','-I',str(r/'sdk-export/include'),'-c',str(src),'-o',str(obj)],env=env,check=True)
subprocess.run([str(z),'ld.lld','-r',str(obj),str(r/'sdk-export/lib/libns_link_v3_band11_candidate.a'),'-o',str(linked)],env=env,check=True)
sys.path.insert(0,str(a.research/'python-deps'))
from elftools.elf.elffile import ELFFile
with linked.open('rb') as f:
 e=ELFFile(f);assert not any(x.name and x['st_shndx']=='SHN_UNDEF' for x in e.get_section_by_name('.symtab').iter_symbols())
(out/'public-example.json').write_text(json.dumps(dict(compiled=True,archive_linked=True,hardware_tested=False,source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),linked_sha256=hashlib.sha256(linked.read_bytes()).hexdigest()),indent=2)+'\n')
print('Public API application example compiled and linked against candidate archive')
