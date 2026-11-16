local original_io,original_os,original_require=io,os,require
local R=assert(NS_PROJECT_ROOT)
assert(loadfile(R.."/device-build/band11-main.lua"));assert(loadfile(R.."/device-build/s4-main.lua"))
local checks=0
local function check(x)checks=checks+1;assert(x,"NS6 Lua check "..checks)end
local function rowtext(row)local v={};for i,x in ipairs(row)do v[i]=tostring(x)end;return"NS6 "..table.concat(v," ").."\n"end
for _,case in ipairs({"normal","ready","firmware","existing","insmod","malformed","held","deleted"})do
 local objects,files,timers={},{},{}
 local modules=case=="existing";local loads,removes=0,0
 local row={table.unpack(NS_S4_ROW)};row[11]=0
 files["/tmp/ns_default_v6_result.txt"]=rowtext(row)
 if case=="ready"then row[3]=0;row[17]=0;row[18]=0 end
 if case=="held"then row[13]=5;row[16]=1;row[17]=0 end
 local function obj(_,props)
  local o={props=props or{},handlers={}}
  function o:clear_flag()end;function o:add_flag()end
  function o:set(v)for k,x in pairs(v)do self.props[k]=x end end
  function o:onevent(e,fn)self.handlers[e]=fn end
  objects[#objects+1]=o;return o
 end
 local lvgl={Object=obj,Label=obj,Font=function()return{}end,HOR_RES=function()return 466 end,VER_RES=function()return 466 end,
  OPA=function(v)return v end,FLAG={SCROLLABLE=1,CLICKABLE=2},ALIGN={CENTER=9},EVENT={CLICKED=1,DELETE=2},
  Timer=function(p)local t={params=p,paused=false};function t:pause()self.paused=true end;function t:resume()self.paused=false end;timers[#timers+1]=t;return t end}
 require=function(n)check(n=="lvgl");return lvgl end
 io={open=function(path,mode)
  if mode=="wb"then files[path]=""end
  return{write=function(_,v)files[path]=(files[path]or"")..v;return true end,read=function()return files[path]or""end,close=function()return true end}
 end}
 os={execute=function(command)
  local cmd,path=command:match("^(.-) > (.+)$");check(cmd and path)
  if cmd=="getprop ro.build.version"then files[path]=case=="firmware"and"wrong"or"3.111.032\n"
  elseif cmd=="lsmod"then files[path]="NAME SIZE\n"..(modules and"ns_default_v6 5000\n"or"")
  elseif cmd:match("^insmod ")then
   loads=loads+1;if case=="insmod"then return false end
   modules=true;files[path]="loaded";files["/tmp/ns_default_v6_result.txt"]=case=="malformed"and"bad"or rowtext(row)
  elseif cmd=="rmmod ns_default_v6"then error("Resident module must not unload")
  else error("unexpected command")end;return true
 end}
 assert(loadfile(R.."/device-build/s4-main.lua"))()
 local button;for _,o in ipairs(objects)do if o.handlers[1]then button=o end end
 check(button);button.handlers[1]()
 if case=="deleted"then objects[1].handlers[2]();timers[1].params.cb();check(removes==0 and modules)
 else if not timers[1].paused then timers[1].params.cb()end
  local text="";for _,o in ipairs(objects)do text=text..tostring(o.props.text or"")end
  local expected=(case=="normal"or case=="existing")and"NS6 PAGE PASS"or(case=="ready"and"ENTRY READY"or(case=="held"and"WAIT / CHECK"or"NS6 PAGE CHECK"))
  check(text:find(expected,1,true))
 end
 if case=="normal"or case=="ready"or case=="held"then check(loads==1 and removes==0 and modules)end
 if case=="firmware"or case=="existing"then check(loads==0 and removes==0)end
 local before=loads;button.handlers[1]();check(loads==before)
end
io,os,require=original_io,original_os,original_require
local rpcfile=assert(io.open(R.."/device/band_default_rpc.lua.in","rb"));local rpc=rpcfile:read("a");rpcfile:close()
local nativefile=assert(io.open(R.."/engineering-build/band_default_bootstrap.bin","rb"));local blob=nativefile:read("a");nativefile:close()
rpc=rpc:gsub("@NATIVE_HEX@",(blob:gsub(".",function(c)return string.format("%02x",c:byte())end))):gsub("@NATIVE_SIZE@",tostring(#blob))
for _,case in ipairs({"normal","ready","not_finished","worker_error","bootstrap_error"})do
 local memory,files={},{};local object=0x3c610000;local mailbox=object+16;local executable=object+144-0x20000000
 local native_calls=0
 local env=setmetatable({lines={},verdict={set=function()end}},{__index=_G})
 env.word=function(a)return memory[a]or 0 end
 env.progress=function()end
 env.original_open=function(path,mode)
  if mode=="wb"then files[path]=""end
  return{write=function(_,v)files[path]=v;return true end,read=function()return files[path]or""end,close=function()return true end}
 end
 local function put(index,v)memory[mailbox+4*index]=v end
 env.execute=function(cmd)
  local addr,value,path=cmd:match("^mw (%x+)=(%x+) > (.+)$")
  if addr then memory[tonumber(addr,16)]=tonumber(value,16);files[path]="OK";return true end
  addr,path=cmd:match("^exec 0x(%x+) > (.+)$");check(addr and path)
  addr=tonumber(addr,16);files[path]=string.format("Calling 0x%08x",addr)
  if(addr&~1)==executable then
   native_calls=native_calls+1;check((memory[mailbox+4]or 0)<=1)
   put(0,0x4e533642);put(1,1);put(2,0x3c410000);put(3,0x3c400000)
   local row={table.unpack(NS_S4_ROW)};row[11]=0
   if case=="ready"then row[3]=0;row[17]=0;row[18]=0 end
   if case=="not_finished"then row[17]=0;row[18]=0 end
   if case=="worker_error"then row[13]=5;row[17]=0 end
   for i,v in ipairs(row)do put(3+i,v)end;put(24,1);put(25,7);put(26,1)
   put(28,case=="bootstrap_error"and 3 or 0)
  end
  return true
 end
 local fn=assert(load(rpc.."\nreturn native_once","NS6 band RPC","t",env))()
 fn(object,128+#blob+32)
 local text=table.concat(env.lines,"\n")
 check(text:find(case=="normal"and"NS6 PAGE PASS"or(case=="ready"and"NS6 ENTRY READY"or"CHECK / WAIT"),1,true))
 check(text:find(case=="ready"and"Manager held"or"Runtime held: YES",1,true))
 check(native_calls>=1)
end
print("{\"s4_host_cases\":8,\"band_rpc_cases\":5,\"lua_syntax_targets\":2,\"checks\":"..checks..",\"failed\":0}")
