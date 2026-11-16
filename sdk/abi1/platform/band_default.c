#include "default_shell_policy.h"
_Static_assert(sizeof(struct ns_sdk_page)<=1024u,"NS6 bridge overlaps resident text");
typedef unsigned int u32;
typedef unsigned char u8;
#define RD(a) (*(volatile u32*)(a))
#define RH(a) (*(volatile unsigned short*)(a))
#define RB(a) (*(volatile u8*)(a))
static __attribute__((always_inline)) inline int bytes(u32 a,u32 n,u32 lo,u32 hi){return a>lo&&a<=hi&&n<=hi-a;}
static __attribute__((always_inline)) inline int span(u32 a,u32 n,u32 lo,u32 hi){return !(a&3u)&&bytes(a,n,lo,hi);}
static __attribute__((always_inline)) inline int ram(u32 a,u32 n,u32 lo,u32 hi){return span(a,n,lo,hi)||span(a,n,0x20000000u,0x2010d9e0u);}
static __attribute__((always_inline)) inline int data(u32 a,u32 n,u32 lo,u32 hi){return ram(a,n,lo,hi)||span(a,n,0x2c0c0000u,0x2cc793c4u);}
static __attribute__((always_inline)) inline u32 mix(u32 h,u32 v){return(h^v)*16777619u;}
static __attribute__((always_inline)) inline int ui(u32 p){
 u32 irq,ctrl;__asm__ volatile("mrs %0,ipsr":"=r"(irq));__asm__ volatile("mrs %0,control":"=r"(ctrl));
 u32 t=RD(0x200b0364u);return !irq&&!(ctrl&1u)&&RD(0x200b0070u)==0x2010d9e0u&&span(t,48u,RD(p+56u),RD(p+60u))&&RD(t+44u)==48u&&t==RD(0x200b0364u);
}
/* out counts2,candidatebits,activehead/first/tail,indexroots3,methods,
 * manager pagecounts2,digest,owned indexnode/key,owned active-list count.
 * No metadata value contents or freed descriptor is dereferenced. */
static __attribute__((noinline,section(".text"))) int view(u32 p,u32 lo,u32 hi,u32*out){
 for(u32 i=0;i<16u;i++)out[i]=0;
 u32 method=RD(0x20084e74u),h=2166136261u,own=RD(p+136u);
 if(!data(method,176u,lo,hi))return 1;
 for(u32 i=0;i<44u;i++)if(RD(method+i*4u)!=RD(0x2ca3b5ccu+i*4u))return 1;
 out[9]=method;out[10]=RD(0x20084e64u);out[11]=RD(0x20084e68u);
 for(u32 k=0;k<2u;k++){
  u32 head=RD(0x20084e58u+k*4u),a,prev,n=0;
  if(head!=(k?0x20084b34u:0x20084b84u))return 2;
  prev=head;a=RD(head+4u);
  if(!k){out[3]=head;out[4]=a;out[5]=RD(head);}
  while(a!=head){
   if(n>=256u||!ram(a,80u,lo,hi)||RD(a)!=prev)return 3;
   u32 id=RH(a+20u);if(id==0x7b1cu)out[2]|=8u<<k;
   if(a==own){if(k)return 3;out[15]++;}
   h=mix(mix(mix(mix(h,a),RD(a)),RD(a+4u)),id);
   prev=a;a=RD(a+4u);n++;
  }
  if(RD(head)!=prev||n!=RH(0x20084e60u+k*2u))return 3;
  out[k]=n;
 }
 for(u32 k=0;k<3u;k++){
  u32 mp= k==2u?RD(0x200cac38u):RD(0x20084e6cu+k*4u),count=0;
  out[6u+k]=mp;if(!span(mp,4u,lo,hi))return 4;
  u32 bs=RD(mp);if(!bs||bs>256u||(bs&(bs-1u))||!span(mp,(bs+1u)*4u,lo,hi))return 4;
  h=mix(h,mp);
  for(u32 b=0;b<bs;b++){
   u32 a=RD(mp+4u+b*4u);
   while(a){
    if(count>=512u||!span(a,20u,lo,hi))return 5;
    u32 key=RD(a+4u),len=RD(a+8u),value=RD(a+12u),calc=5381u,same=(len==17u);
    if(!len||len>128u||!bytes(key,len,lo,hi))return 6;
    for(u32 j=0;j<len;j++){
     u32 c=RB(key+j);
     if(j+1u==len){if(c)return 6;}else{if(!c)return 6;calc=calc*33u+c;}
     if(same&&c!=RB(p+224u+j))same=0;
     h=mix(h,c);
    }
    if(calc!=RD(a)||(calc&(bs-1u))!=b)return 7;
    if(same)out[2]|=1u<<k;
    if(k<2u){
     if(!ram(value,80u,lo,hi))return 8;
     u32 nm=RD(value+12u);
     if(!bytes(nm,len,lo,hi)&&!bytes(nm,len,0x20000000u,0x2010d9e0u)&&!bytes(nm,len,0x2c0c0000u,0x2cc793c4u))return 8;
     for(u32 j=0;j<len;j++)if(RB(nm+j)!=RB(key+j))return 8;
     if(own&&value==own){if(k||!same||out[13])return 8;out[13]=a;out[14]=key;}
    }
    h=mix(mix(mix(mix(h,a),key),value),RD(a+16u));count++;a=RD(a+16u);
   }
  }
  if(k<2u&&count!=out[k]+((!k&&RD(p+132u)==1u)?1u:0u))return 9;
 }
 out[12]=mix(mix(h,RD(0x20084e60u)),out[10]);return 0;
}

#include "band_default_resident.inc"
#include "band_default_launcher.inc"
static void controller(u32 timer);
static void ns6_band_tick(u32 timer);
__attribute__((used,section(".text.entry"),aligned(4))) void owned_callback(u32 timer){ns6_band_tick(timer);}
static __attribute__((noinline,section(".text"))) int reg_view(u32 p,u32 lo,u32 hi,u32*out){
 for(u32 i=0;i<4u;i++)out[i]=0;
 if(RD(0x20084e3cu)!=0x20084dc4u)return 1;
 if(RD(0x20084e48u)!=0xc694c45u||RD(0x20084e4cu)!=0xc695221u)return 2;
 u32 a=RD(0x20084e08u),prev=0x20084dc4u,h=2166136261u;
 while(a){
  if(out[3]>=2048u||!ram(a,108u,lo,hi)||RD(a+64u)!=prev)return 3;
  if(RD(a+20u)==0x7b1c0001u){if(a==p+256u)out[0]++;else out[1]++;}
  h=mix(mix(mix(mix(h,a),RD(a+20u)),prev),RD(a+68u));
  prev=a;a=RD(a+68u);out[3]++;
 }
 out[2]=h;return 0;
}
static __attribute__((noinline,section(".text"))) int page_map_ok(u32 p,u32 lo,u32 hi){
 u32 table=RD(p+400u),mp=RD(p+404u),found=0;
 if(!table)return !mp;
 if(!span(table,12u,lo,hi)||!(table+12u<=p||table>=p+32768u)||RH(table)>1u||RD(table+4u)!=mp)return 0;
 if(!mp)return !RH(table);
 if(!span(mp,68u,lo,hi)||!(mp+68u<=p||mp>=p+32768u)||RD(mp)!=16u)return 0;
 if(mp==RD(0x20084e6cu)||mp==RD(0x20084e70u)||mp==RD(0x200cac38u))return 0;
 for(u32 i=0;i<16u;i++){
  u32 a=RD(mp+4u+i*4u);if(!a)continue;
  if(found||!span(a,20u,lo,hi)||!(a+20u<=p||a>=p+32768u)||RD(a+8u)!=7u||RD(a+12u)!=p+256u||RD(a+16u))return 0;
  u32 key=RD(a+4u),h=5381u;
  if(!span(key,7u,lo,hi)||!(key+7u<=p||key>=p+32768u))return 0;
  if(!(key+7u<=a||key>=a+20u)||!(key+7u<=mp||key>=mp+68u)||!(a+20u<=mp||a>=mp+68u))return 0;
  for(u32 j=0;j<7u;j++){u32 c=RB(key+j);if(c!=RB(0x2cb40eefu+j))return 0;if(j<6u)h=h*33u+c;}
  if(RD(a)!=h||(h&15u)!=i)return 0;found++;
 }
 return found==RH(table)&&(!found||RD(table+8u)==p+256u);
}
static __attribute__((noinline,section(".text"))) int page_prepare(u32 p,u32 lo,u32 hi){
 u32 rv[4],e=reg_view(p,lo,hi,rv),page=p+256u;
 if(e)return 40u+e;if(rv[0]||rv[1])return 44;
 RD(p+412u)=RD(0x20084e08u);RD(p+416u)=rv[2];
 for(u32 j=0;j<4u;j++){RD(p+472u+j*4u)=RD(0x200c2d44u+j*4u);RD(p+488u+j*4u)=RD(0x200c2d34u+j*4u);}
 if((RD(p+180u)&65535u)==65535u)return 45;
 u32 cb=RD(p+408u),start=p+4096u-0x20000000u;
 if(!(cb&1u)||(cb&~1u)<start||(cb&~1u)>=start+28672u)return 57;
 for(u32 j=0;j<5u;j++){u32 c=RD(p+700u+j*4u);if(!(c&1u)||(c&~1u)<start||(c&~1u)>=start+28672u)return 57;}
 for(u32 j=0;j<27u;j++)RD(page+j*4u)=0;
 RD(page+16u)=0x2cb40eefu;RD(page+20u)=0x7b1c0001u;RD(page+24u)=6u;RD(page+40u)=0x00020404u;RD(page+52u)=RD(p+408u);
 RD(page+76u)=RD(p+700u);RD(page+80u)=RD(p+712u);RD(page+88u)=RD(p+704u);RD(page+92u)=RD(p+716u);RD(page+96u)=RD(p+704u);RD(page+100u)=RD(p+708u);
 u32 table=((u32(*)(u32))0xc3514edu)(12u);RD(p+400u)=table;
 if(!table)return 46;
 if(!span(table,12u,lo,hi)||!(table+12u<=p||table>=p+32768u))return 47;
 for(u32 j=0;j<3u;j++)RD(table+j*4u)=0;
 u32 mp=((u32(*)(u32))0xc6d9783u)(10u);RD(p+404u)=mp;RD(table+4u)=mp;
 if(!mp)return 48;
 if(!page_map_ok(p,lo,hi))return 49;
 if(((int(*)(u32,u32,u32))0xc6d16a1u)(mp,0x2cb40eefu,page))return 50;
 RH(table)=1;RD(table+8u)=page;
 if(!page_map_ok(p,lo,hi))return 49;
 RD(RD(p+136u)+56u)=table;return 0;
}
static __attribute__((noinline,section(".text"))) void private_free(u32 p){
 u32 app=RD(p+136u),name=RD(p+140u),lo=RD(0x3c356b5cu),hi=RD(0x3c356b60u);
 if(RD(p+420u)||RD(p+728u)||!page_map_ok(p,lo,hi))return;
 launch_private_free(p);
 if(RD(p+404u))((void(*)(u32))0xc8e858du)(RD(p+404u));
 RD(p+404u)=0;
 if(RD(p+400u))((void(*)(u32))0xc34cc7du)(RD(p+400u));
 RD(p+400u)=0;RD(p+408u)=0;RD(p+308u)=0;
 if(app)((void(*)(u32))0xc34cc7du)(app);
 RD(p+136u)=0;
 if(name)((void(*)(u32))0xc34cc7du)(name);
 RD(p+140u)=0;RD(p+144u)=0;RD(p+128u)=1;
}
static __attribute__((noinline,section(".text"))) void controller(u32 timer){
 if(!timer)return;u32 p=RD(timer+12u);if(!p||RD(p)!=0xb641c0deu)return;
 u32 ctrl,irq;__asm__ volatile("mrs %0,control":"=r"(ctrl));__asm__ volatile("mrs %0,ipsr":"=r"(irq));
 u32 n=++RD(p+4u),t=RD(0x200b0364u),e=0;RD(p+12u)=timer;RD(p+16u)=ctrl;RD(p+20u)=irq;
 if(RD(0x200b0070u)==0x2010d9e0u&&span(t,48u,RD(p+56u),RD(p+60u)))RD(p+48u)=RD(t+44u);
 if(!ui(p)){e=1;goto done;}
 if(RD(0x200b2330u)!=0x3c356b40u){e=2;goto done;}
 u32 lo=RD(0x3c356b5cu),hi=RD(0x3c356b60u),v[16],app=RD(p+136u),name=RD(p+140u);
 if(lo<0x3c000000u||hi<=lo||hi>0x3d000000u){e=2;goto done;}
 if(n==1u){
  RD(p+224u)=0x2e313162u;  RD(p+228u)=0x6974616eu;  RD(p+232u)=0x702e6576u;  RD(p+236u)=0x36656761u;RB(p+240u)=0;
  e=view(p,lo,hi,v);if(e){e+=20;RD(p+128u)=1;goto done;}
  if(v[2]||v[0]>=256u){e=3;RD(p+128u)=1;goto done;}
  RD(p+148u)=v[3];RD(p+152u)=v[4];RD(p+156u)=v[5];RD(p+160u)=RD(0x20084e60u);
  for(u32 j=0;j<6u;j++)RD(p+164u+j*4u)=v[6u+j];
  RD(p+188u)=v[12];
  app=((u32(*)(u32))0xc3514edu)(80u);RD(p+136u)=app;
  if(!app){e=4;RD(p+144u)=0;RD(p+128u)=1;goto done;}
  if(!span(app,80u,lo,hi)||!(app+80u<=p||app>=p+32768u)){e=5;goto done;}
  for(u32 j=0;j<20u;j++)RD(app+j*4u)=0;
  name=((u32(*)(u32))0xc3514edu)(17u);RD(p+140u)=name;
  if(!name){e=6;private_free(p);goto done;}
  if(!span(name,17u,lo,hi)||!(name+17u<=p||name>=p+32768u)||!(name+17u<=app||name>=app+80u)){e=5;goto done;}
  for(u32 j=0;j<17u;j++)RB(name+j)=RB(p+224u+j);
  u32 cb=RD(p+144u),start=p+4096u-0x20000000u;
  if(!(cb&1u)||(cb&~1u)<start||(cb&~1u)>=start+28672u){e=5;private_free(p);goto done;}
  RD(app+12u)=name;RD(app+20u)=0x01087b1cu;RD(app+24u)=1;RD(app+68u)=cb;
  e=page_prepare(p,lo,hi);
  if(e){private_free(p);goto done;}
  e=launch_prepare(p,lo,hi);if(e){private_free(p);goto done;}
  if(((int(*)(u32,u32,u32))0xc6d16a1u)(v[6],name,app)){
   e=7;int check=view(p,lo,hi,v);
   if(!check&&!v[2]&&!v[15]&&v[12]==RD(p+188u))private_free(p);
   goto done;
  }
  RD(p+132u)=1;
  u32 head=RD(p+148u),first=RD(p+152u);
  if(RD(head+4u)!=first||RD(head)!=RD(p+156u)||RD(first)!=head||RD(0x20084e60u)!=RD(p+160u)){
   e=8;
   int check=view(p,lo,hi,v);
   if(!check&&v[2]==1u&&!v[15]&&v[13]){
    if(!((int(*)(u32,u32))0xc6d10d1u)(v[6],name)){
     RD(p+132u)=0;
     check=view(p,lo,hi,v);
     if(!check&&!v[2]&&!v[15])private_free(p);
    }
   }
   goto done;
  }
  RD(app)=head;RD(app+4u)=first;
  __asm__ volatile("dsb sy" ::: "memory");
  RD(first)=app;RD(head+4u)=app;RD(p+132u)=3;
  *(volatile unsigned short*)0x20084e60u=(unsigned short)((RD(p+160u)&65535u)+1u);RD(p+132u)=7;
  RD(p+80u)=1;
  RH(0x20084e64u)=(unsigned short)((RD(p+180u)&65535u)+1u);RD(p+456u)=1;
  ((void(*)(u32))0xc694c45u)(p+256u);RD(p+420u)=1;
  u32 rv[4],result=0;
  int pagecheck=reg_view(p,lo,hi,rv);
  if(pagecheck||rv[0]!=1u||rv[1]||RD(p+436u)!=1u){e=51;goto done;}
  RD(p+432u)=(((u32(*)(u32))0xc693e8du)(0x7b1c0001u)==p+256u);
  RD(p+444u)=(((u32(*)(u32))0xc6aa9b9u)(0x7b1cu)==p+256u);
  RD(p+448u)=(((u32(*)(u32,u32))0xc6aa9d7u)(0x7b1cu,1u)==p+256u);
  RD(p+452u)=(!((int(*)(u32,u32,u32))0xc6d1161u)(RD(p+404u),0x2cb40eefu,(u32)&result)&&result==p+256u);
  if(!RD(p+432u)||!RD(p+444u)||!RD(p+448u)||!RD(p+452u)){e=52;goto done;}
  e=view(p,lo,hi,v);if(e){e+=20;goto done;}
  if(v[2]!=9u||v[15]!=1u||!v[13]){e=9;goto done;}
  RD(p+104u)=(v[0]==(RD(p+160u)&65535u)+1u);
  RD(p+88u)=(((u32(*)(u32))0xc6aa995u)(0x7b1cu)==app);
  RD(p+92u)=(((u32(*)(u32))0xc6aab09u)(name)==app);
  if(!RD(p+104u)||!RD(p+88u)||!RD(p+92u)){e=9;goto done;}
  e=launch_publish(p,lo,hi);if(e)goto done;
  resident_text(p);RD(p+512u)=1;RD(p+516u)=1;resident_poll(p,lo,hi);
 }else if(RD(p+772u)){
  resident_poll(p,lo,hi);
 }else{e=13;goto done;}

done:
 if(!RD(p+136u)&&!RD(p+140u)&&!RD(p+132u)&&!RD(p+400u)&&!RD(p+404u)&&!RD(p+420u)){RD(p+408u)=0;RD(p+308u)=0;RD(p+732u)=0;RD(p+128u)=1;}
 if(e&&!RD(p+76u))RD(p+76u)=e;
 RD(p+120u)=RD(p+136u);RD(p+124u)=RD(p+140u);
 if(e||RD(p+76u)){ns5page_pause((void*)(p+1024u));((void(*)(u32))0xc3abdd9u)(timer);RD(p+32u)=1;}
 __asm__ volatile("dsb sy" ::: "memory");
}
__attribute__((used,section(".text.signal"),aligned(4))) u32 app_signal(u32 original,u32 signal,u32 arg){
 u32 p;__asm__ volatile("adr.w %0, owned_callback":"=r"(p));p=(p&~1u)+0x20000000u-4096u;
 if(RD(p)!=0xb641c0deu)return 1u;
 if(original!=RD(p+136u)||!ui(p)){err(p,14);return 1u;}
 if(signal==0u)RD(p+608u)++;else if(signal==3u)RD(p+612u)++;
 else if(signal==5u){RD(p+96u)++;RD(p+100u)++;RB(original+22u)|=8u;RD(p+136u)=0;err(p,96);}
 else {RD(p+824u)=signal;RD(p+828u)=arg;}
 return 1u;
}

__attribute__((used,section(".text.pagesignal"),aligned(4))) u32 page_signal(u32 page,u32 signal,u32 arg){
 u32 p=page-256u;if(RD(p)!=0xb641c0deu||RD(page+20u)!=0x7b1c0001u)return 0;
 if(!ui(p)){err(p,55);return 0;}
 if(arg)RD(p+828u)=arg;
 if(signal==1u||signal==2u){if(arg||RD(page+48u)||RD(page+36u))err(p,55);if(signal==1u)RD(p+436u)++;else RD(p+440u)++;}
 else if(signal!=3u&&signal!=4u&&signal!=5u&&signal!=6u&&signal!=7u&&signal!=8u&&signal!=20u&&signal!=21u&&signal!=22u)err(p,56);
 return 0;
}

/* NS6 additions: no automatic app/page withdrawal. The installed manager and
 * UI-list references stay live until reboot or a separately qualified stop. */
static struct ns_sdk_page *pgsession(u32 p){return (void*)(p+1024u);}
static void ns6_band_tick(u32 timer){
 if(!timer)return;u32 p=RD(timer+12u);
 if(!p||RD(p)!=0xb641c0deu||!ui(p))return;
 struct ns_sdk_page *s=pgsession(p);
 if(!s->flow.initialized){struct ns_ui_context c;if(!nsp_ui_capture(&c)||ns6shell_begin(s,&c)!=NS_OK){err(p,160);return;}}
 controller(timer);
 if(!RD(p+76u)){ns5page_poll(s);if(s->flow.error||s->ble.error)err(p,161);}
 if(RD(p+304u)&&RD(p+552u)==RD(p+304u)&&RD(p+556u)&&s->flow.visible){
  char *message=(void*)(p+2048u);ns6page_text(s,message);
  /* Copying text API; source storage remains inside the resident manager. */
  u32 digest=2166136261u;for(u32 i=0;message[i]&&i<126u;i++)digest=mix(digest,(u8)message[i]);
  if(digest!=RD(p+2520u)){((void(*)(u32,u32))0xc3b38b1u)(RD(p+556u),(u32)message);RD(p+2520u)=digest;}
 }
 u32 quiet=RD(p+808u)==RD(p+560u)&&RD(p+560u)&&!RD(p+304u)&&!RD(p+292u);
 u32 entry=(RD(p+812u)==3u&&RD(p+816u)==3u)?7u:0u;
 u32 pass=ns6page_pass(s,RD(p+560u),RD(p+564u),RD(p+576u),RD(p+580u),quiet,entry)&&!RD(p+76u);
 RD(p+8u)=pass?0xb641a739u:0;
 volatile u32 *v=(void*)(p+2304u);struct ns_ble_backend *b=&s->ble;
 v[0]=0x4e533652u;v[1]=RD(p+4u);v[2]=RD(p+560u)|(RD(p+564u)<<16);v[3]=RD(p+576u)|(RD(p+580u)<<16);
 v[4]=b->created|(b->released<<16);v[5]=b->scan_start|(b->scan_stop<<16);v[6]=b->adv_start|(b->adv_stop<<16);
 v[7]=b->barriers|(b->joined<<16);v[8]=b->callbacks;v[9]=b->overlaps;v[10]=s->flow.deferred;v[11]=s->flow.wanted_epoch;
 v[12]=RD(p+76u);v[13]=b->error;v[14]=quiet;v[15]=entry;v[16]=pass;v[17]=pass?0x4e533650u:0;
 v[18]=ns_state(&b->controller.manager);v[19]=1;
 if(RD(p+76u))ns5page_pause(s);
}
