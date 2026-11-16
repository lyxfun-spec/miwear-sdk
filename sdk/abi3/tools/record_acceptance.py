"""Associate G4 raw text + PC evidence + exact package. No fabricated acceptance."""
import argparse,hashlib,json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def parse(text):
 expected={'Cases passed/started':[4,4],'Sessions open/destroy':[4,4],'Live sessions/bytes':[0,0],'Connect/disconnect rounds':[10,8],'Tokens created/collected':[20,20],'Exit/join/barrier':[4,4,10],'Expected ATT error':[1],'Queue BUSY':[8],'Callback UI/active':[48,0],'Closed':[4],'Error':[0],'Fixed TX cap':[20]}
 if not re.search(r'^G4R2 SDK PASS$',text,re.M) or 'CHECK' in text:raise ValueError('G4 PASS missing')
 rows={}
 for k,v in expected.items():
  found=re.findall('^'+re.escape(k)+r': ([0-9/]+)$',text,re.M)
  if len(found)!=1 or [int(x) for x in found[0].split('/')]!=v:raise ValueError(k)
  rows[k]=v
 found=re.findall(r'^Read/write/notify: (\d+)/(\d+)/(\d+)$',text,re.M)
 if len(found)!=1 or int(found[0][0]) not in [9,10] or found[0][1:]!=('32','32'):raise ValueError('data')
 quiet=re.findall(r'^Quiet ticks: (\d+)$',text,re.M)
 if len(quiet)!=1 or int(quiet[0])<20:raise ValueError('quiet')
 for k,v in [('Manager timer','STOPPED'),('Module/runtime','HELD')]:
  if re.findall('^'+k+r': (.*)$',text,re.M)!=[v]:raise ValueError(k)
 return rows

def main():
 p=argparse.ArgumentParser();p.add_argument('--text',type=Path,required=True);p.add_argument('--pc-log',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 if a.out.exists():raise FileExistsError(a.out)
 text=a.text.read_text(encoding='utf-8-sig');rows=parse(text);peer=json.loads(a.pc_log.read_text(encoding='utf-8'));events=peer['events']
 build=json.loads((R/'build/build.json').read_text());lib=R/'sdk-export/lib/libns_link_v3_band11_candidate.a'
 if peer.get('fixture_sha256')!=build['sources']['peer/windows_peer.py'] or hashlib.sha256(lib.read_bytes()).hexdigest()!=build['sdk_library_sha256']:raise ValueError('fixture/library changed')
 if peer.get('service')!='91d10001-d0b2-4cf6-9136-f16db55b6e11' or peer.get('channel')!='91d10002-d0b2-4cf6-9136-f16db55b6e11':raise ValueError('wrong channel')
 writes=[e for e in events if e['kind']=='write'];normal=[e for e in writes if e.get('valid')];bad=[e for e in writes if e.get('expected_failure')]
 wanted={bytes((0x40+r+i*23+j)&255 for j in range(1 if i==0 else 20 if i==2 else 8)).hex() for r in range(1,9) for i in range(4)}
 if len(writes)!=33 or len(normal)!=32 or {e['hex'] for e in normal}!=wanted or len(bad)!=1:raise ValueError('PC writes')
 if len([e for e in events if e['kind']=='read'])<9:raise ValueError('PC reads')
 if len([e for e in events if e['kind']=='notify' and any(r['status']=='SUCCESS' for r in e['results'])])!=32:raise ValueError('PC notifications')
 subs=[e['count'] for e in events if e['kind']=='subscribers']
 if subs.count(1)<10 or subs.count(0)<10 or any(n not in [0,1] for n in subs):raise ValueError('PC subscription cycles')
 if any(e['kind'].endswith('_error') for e in events):raise ValueError('PC errors')
 sha=lambda b:hashlib.sha256(b).hexdigest();manifest=json.loads((R/'device-build/manifest.json').read_text());face=R/'device-build/Band11NFC-G4R2-SDK.face'
 if sha(face.read_bytes())!=manifest['files'][face.name]['sha256']:raise ValueError('package changed')
 a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(dict(g4_foreground_resource_tested=True,background_tested=False,raw_text=text,counters=rows,pc_raw=peer,pc_sha256=sha(a.pc_log.read_bytes()),artifact=manifest,association='User transcription and PC log associated to delivered package; no device binary hash readback'),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('G4 foreground resource evidence recorded; background/power remain unverified')
if __name__=='__main__':main()
