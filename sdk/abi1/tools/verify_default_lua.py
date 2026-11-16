"""Text-only NS6 Lua tests; no device, GUI or image access."""
from pathlib import Path
import argparse,json,subprocess,hashlib
R=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--lua',type=Path,required=True);args=a.parse_args()
row=json.loads((R/'engineering-build/s4-ui-verification.json').read_text())['cases'][0]['report']
script='NS_PROJECT_ROOT='+json.dumps(R.as_posix())+'\nNS_S4_ROW={'+','.join(map(str,row))+'}\nassert(loadfile(NS_PROJECT_ROOT.."/tests/default_lua_host.lua"))()\n'
entry=R/'engineering-build/lua_default_check.lua';entry.write_text(script)
result=json.loads(subprocess.check_output([str(args.lua.resolve()),str(entry)],text=True,cwd=R));result['hardware_tested']=False
result['lua_sha256']={n:hashlib.sha256((R/'device-build'/n).read_bytes()).hexdigest() for n in ['s4-main.lua','band11-main.lua']}
(R/'device-build/lua-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
