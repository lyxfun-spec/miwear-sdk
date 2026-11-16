"""Validate limited same-section REL32 normalization; no target execution."""
from pathlib import Path
from io import BytesIO
import json,sys,struct,importlib.util,argparse,hashlib
R=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--research',type=Path,required=True);args=a.parse_args();sys.path.insert(0,str(args.research/'python-deps'))
from elftools.elf.elffile import ELFFile
spec=importlib.util.spec_from_file_location('default_builder',R/'tools/build_default.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
raw=(R/'engineering-build/ns_default_v6.preload.elf').read_bytes();normalized,proof=m.normalize_local_rel32(raw);assert len(proof)>0
original=ELFFile(BytesIO(raw));after=ELFFile(BytesIO(normalized));assert all(x['r_info_type']!=3 for s in after.iter_sections() if s['sh_type']=='SHT_REL' for x in s.iter_relocations())
rel=next(s for s in original.iter_sections() if s['sh_type']=='SHT_REL' and any(x['r_info_type']==3 for x in s.iter_relocations()))
symbols=original.get_section(rel['sh_link']);cross=next(i for i,x in enumerate(symbols.iter_symbols()) if isinstance(x['st_shndx'],int) and original.get_section(x['st_shndx']).name=='.bss')
idx=next(i for i,x in enumerate(rel.iter_relocations()) if x['r_info_type']==3);at=rel['sh_offset']+idx*rel['sh_entsize']
cases=[{'case':'same-section references resolved with proof','passed':True}]
for name,mutate in [('cross-section reference',lambda b:struct.pack_into('<I',b,at+4,(cross<<8)|3)),('unaligned relocation',lambda b:struct.pack_into('<I',b,at,1)),('out-of-range relocation',lambda b:struct.pack_into('<I',b,at,0xfffffff0))]:
 b=bytearray(raw);mutate(b)
 try:m.normalize_local_rel32(b)
 except AssertionError:cases.append({'case':'reject '+name,'passed':True})
 else:raise AssertionError('unsafe relocation accepted: '+name)
# Loader-specific transform must exactly match the installed ELF, except entry.
build=json.loads((R/'engineering-build/build.json').read_text());struct.pack_into('<I',normalized,24,build['targets']['s4']['entry']);assert normalized==(R/'engineering-build/ns_default_v6.elf').read_bytes()
r={'hardware_tested':False,'cases':cases,'resolved_local_relocations':len(proof),'preload_sha256':hashlib.sha256(raw).hexdigest(),'module_sha256':hashlib.sha256(normalized).hexdigest()}
(R/'engineering-build/relocation-verification.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
