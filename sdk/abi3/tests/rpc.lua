local root=assert(G1_ROOT)
local f=assert(io.open(root..'/device/g1_rpc.lua.in','rb'));local rpc=f:read('a');f:close()
rpc=rpc:gsub('@NATIVE_HEX@','00f002b800000000'):gsub('@NATIVE_SIZE@','8')
local base={4,0,4,4,4,4,0,0,10,8,10,32,32,20,20,4,4,10,1,8,48,0,4,1,20,0}
local tests={{'valid'}, {'token_missing',18,19}, {'owner_live',10,1}, {'join_missing',19,3}, {'write_missing',15,31}, {'callback_active',25,1}, {'error',5,54}, {'timer_live',29,1}, {'bootstrap_error',31,2}, {'never_done',27,0}, {'read_min',14,9}, {'quiet_short',28,19}}
for _,t in ipairs(tests)do
 local v={};for i=1,32 do v[i]=0 end;v[1]=0x47340002
 for i,n in ipairs(base)do v[i+3]=n end
 if t[2]then v[t[2]]=t[3]end
 local object=0x3c400000;local mailbox=object+16;local last,calls,stop,diagnostic=0,0,false,false
 lines={};local verdict_text='';verdict={set=function(self,x)verdict_text=x.text end};progress=function()end
 execute=function(command)
  calls=calls+1;assert(calls<300,'unbounded polling')
  local addr,val=command:match('mw (%x+)=(%x+)')
  if addr then if tonumber(addr,16)==mailbox+4 then if tonumber(val,16)==2 then stop=true end;if tonumber(val,16)==3 then diagnostic=true end end;return true end
  last=assert(tonumber(command:match('exec 0x(%x+)'),16));return true
 end
 original_open=function()return{write=function()return true end,close=function()return true end,read=function()return string.format('Calling 0x%08x',last)end}end
 word=function(a)if a==object+148 then return mailbox end;if diagnostic then return a==mailbox and 0x47340002 or 0 end;return assert(v[(a-mailbox)//4+1])end
 assert(load(rpc..'\nreturn native_once','g4-rpc','t',_ENV))()(object,512)
 assert((verdict_text=='G4R2 SDK PASS')==(t[1]=='valid' or t[1]=='read_min'),t[1])
 if t[1]=='never_done'then assert(stop)end
 if t[1]=='error'then assert(calls<50,'failure must leave polling promptly')end
end
assert(loadfile(root..'/device-build/band11-main.lua'))
print('G4 Lua verdict/cancel cases: '..#tests..' PASS; packaged syntax PASS')
