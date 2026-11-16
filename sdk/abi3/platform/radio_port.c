#include "ns_radio_port.h"
#define R_RD(a) (*(volatile ns_u32*)(a))
#define R_RB(a) (*(volatile unsigned char*)(a))
#define R_FN(a,t) ((t)((a)|1u))
static ns_u32 rcritical(void){ns_u32 old,v=128;__asm__ volatile("mrs %0,basepri":"=r"(old));__asm__ volatile("msr basepri_max,%0"::"r"(v):"memory");return old;}
static void runcritical(ns_u32 v){__asm__ volatile("msr basepri,%0"::"r"(v):"memory");}
#if NS_TARGET_BAND
#define R_HEAD 0x20083274u
#define R_ENTRY 112u
#define R_NAME 33u
#define R_INSTANCE 24u
#define R_INTERNAL 52u
#define R_EXTERNAL 56u
#define R_QUEUE 76u
#define R_FRAME 720u
#define R_RPC 0xc46a18cu
#define R_RUNNING 184u
#define R_TIMER_HEAD 0x200bd108u
#include "radio_guards_band11.h"
enum ns_target nsr_target(void){return NS_BAND11;}
#else
#define R_HEAD 0x3c21cdd4u
#define R_ENTRY 100u
#define R_NAME 25u
#define R_INSTANCE 20u
#define R_INTERNAL 48u
#define R_EXTERNAL 52u
#define R_QUEUE 72u
#define R_FRAME 624u
#define R_RPC 0x2c7d47e6u
#define R_RUNNING 180u
#define R_TIMER_HEAD 0x3c2c5eb4u
#include "radio_guards_s4.h"
enum ns_target nsr_target(void){return NS_S4;}
#endif
ns_u32 nsr_guards(void){return radio_check_guards();}
static ns_u32 rlookup(const struct ns_ui_context *c,struct ns_borrowed *out) {
 ns_u32 pid=R_RD(c->group+(NS_TARGET_BAND?0u:4u)),node=R_RD(R_HEAD+4u),prev=R_HEAD,found=0;
 if(!pid || (pid&0x80000000u))return 0;
 for(ns_u32 i=0;node!=R_HEAD && i<64u;i++) {
  if(!nsp_valid((void*)node,R_ENTRY) || R_RD(node)!=prev)return 0;
  if(R_RD(node+8u)==pid && R_RB(node+R_NAME)=='l' && R_RB(node+R_NAME+1u)=='o' &&
     R_RB(node+R_NAME+2u)=='c' && R_RB(node+R_NAME+3u)=='a' && R_RB(node+R_NAME+4u)=='l' && !R_RB(node+R_NAME+5u)) {
   if(found)return 0;
#if NS_TARGET_BAND
   if(R_RD(node+12u) || R_RD(node+28u))return 0;
#endif
   ns_u32 client=R_RD(node+R_INSTANCE);
   if(!nsp_valid((void*)client,NS_TARGET_BAND?212u:176u))return 0;
   ns_u32 internal=R_RD(client+R_INTERNAL),external=R_RD(client+R_EXTERNAL);
   if(!nsp_valid((void*)internal,NS_TARGET_BAND?528u:512u))return 0;
   ns_u32 data=R_RD(internal);
   if(!nsp_valid((void*)data,196u) || !R_RB(data+R_RUNNING))return 0;
   if(external && !nsp_valid((void*)external,NS_TARGET_BAND?528u:512u))return 0;
#if NS_TARGET_BAND
   if(!external)return 0; /* B36 exact external UI48 callback contract. */
#endif
   out->entry=node;out->client=client;out->internal=internal;out->external=external;out->pid=pid;found=1;
  }
  prev=node;node=R_RD(node+4u);
 }
 return node==R_HEAD && R_RD(R_HEAD)==prev && found;
}
ns_u32 nsr_borrow(const struct ns_ui_context *c,struct ns_borrowed *out) {
 ns_u32 saved=rcritical();ns_u32 ok=rlookup(c,out);runcritical(saved);return ok;
}
ns_u32 nsr_live(const struct ns_ui_context *c,const struct ns_borrowed *expected) {
 struct ns_borrowed now;ns_u32 saved=rcritical();ns_u32 ok=rlookup(c,&now);
 ok=ok && now.entry==expected->entry && now.client==expected->client && now.internal==expected->internal && now.external==expected->external && now.pid==expected->pid;
 runcritical(saved);return ok;
}
ns_u32 nsr_callback_context(const struct ns_ui_context *c,const struct ns_borrowed *b,ns_u32 *tid) {
 if(b->external){*tid=c->tid;return nsp_ui_matches(c);}
 return nsp_worker_identity(c,tid);
}
ns_u32 nsr_rpc(const struct ns_borrowed *b,void *packet,ns_u32 cmd){return R_FN(R_RPC,ns_u32(*)(ns_u32,void*,ns_u32))(b->client,packet,cmd);}
ns_u32 nsr_barrier(const struct ns_ui_context *c,const struct ns_borrowed *b,ns_u32 sync[10]) {
 if(!nsr_live(c,b))return 0;
 sync[0]=NS_TARGET_BAND?0xc91ca03u:0x2c7c7a8fu;sync[1]=0;
#if NS_TARGET_BAND
 if(R_FN(0xc8bbb10u,int(*)(void*))(&sync[2]))return 0;
 R_FN(0xc465cd4u,void(*)(ns_u32,ns_u32,void*))(b->internal,0xc464155u,sync);
 R_FN(0xc3f513cu,void(*)(void*))(&sync[2]);R_FN(0xc3f5104u,void(*)(void*))(&sync[2]);
#else
 if(R_FN(0x2c8a9d60u,int(*)(void*))(&sync[2]))return 0;
 R_FN(0x2c7cbd04u,void(*)(ns_u32,ns_u32,void*))(b->internal,0x2c7c93d5u,sync);
 R_FN(0x2c485930u,void(*)(void*))(&sync[2]);R_FN(0x2c48588cu,void(*)(void*))(&sync[2]);
#endif
 return 1;
}
enum ns_result nsr_queue_clear(const struct ns_ui_context *c,const struct ns_borrowed *b,ns_u32 scan,ns_u32 adv) {
 if(!nsp_ui_matches(c) || !nsr_live(c,b))return NS_INVALID;
#if !NS_TARGET_BAND
 /* Match the accepted S4 split: with no external dispatch loop, own callbacks
  * finish on the internal loop covered by the completed ROM FIFO barrier.
  * No external delivery source was published, so do not touch its unused
  * queue/mutex or mistake uninitialized external state for a live queue. */
 if(!b->external)return NS_OK;
#endif
 ns_u32 h=b->client+R_QUEUE,node,prev,n=0,pending=0;enum ns_result result=NS_INVALID;
#if NS_TARGET_BAND
 ns_u32 saved=rcritical();
#else
 R_FN(0x2c483a20u,void(*)(void*))( (void*)(b->client+80u));
#endif
 node=R_RD(h+4u);prev=h;
 while(node!=h && n++<1024u) {
  if(!nsp_valid((void*)node,R_FRAME) || R_RD(node)!=prev)goto done;
  ns_u32 token=0,known=1;
#if NS_TARGET_BAND
  if(R_RD(node+8u)!=b->client)goto done;
  ns_u32 body=node+12u,code=R_RD(body),off=0;
  if(code==0x167u || code==0x117u)off=60u;
  else if(code==0x168u)off=340u;else if(code==0x169u)off=356u;
  else if(code==0x118u)off=76u;else if(code==0x1060001u)off=68u;else known=0;
  if(off){token=R_RD(body+off);if(R_RD(body+off+4u))known=0;}
#else
  ns_u32 code=R_RB(node+12u)|((ns_u32)R_RB(node+13u)<<8);
  if(code==0x138u || code==0xfbu)token=R_RD(node+56u);
  else if(code==0xfcu)token=R_RD(node+64u);else if(code==0x139u)token=R_RD(node+0x148u);
  else if(code==0x13au)token=R_RD(node+0x150u);else known=0;
#endif
  if(!known || (token && (token==scan || token==adv)))pending=1;
  prev=node;node=R_RD(node+4u);
 }
 if(node!=h || R_RD(h)!=prev)goto done;
 result=pending?NS_BUSY:NS_OK;
done:
#if NS_TARGET_BAND
 runcritical(saved);
#else
 R_FN(0x2c483a3cu,void(*)(void*))((void*)(b->client+80u));
#endif
 return result;
}
enum ns_result nsr_timers_clear(const void *owner,ns_u32 bytes,const ns_u32 *sc,const ns_u32 *ac,ns_u32 scan,ns_u32 adv) {
 if(R_RD(R_TIMER_HEAD)!=24u)return NS_INVALID;
 ns_u32 node=R_RD(R_TIMER_HEAD+4u),prev=0,p=(ns_u32)owner,pending=0;
 for(ns_u32 n=0;node && n<8192u;n++) {
  if(!nsp_valid((void*)node,32u) || R_RD(node+24u)!=prev)return NS_INVALID;
  ns_u32 cb=R_RD(node+8u),ud=R_RD(node+12u);
  if((ud>=p && ud-p<bytes) || (ud && (ud==scan || ud==adv)))pending=1;
  for(ns_u32 i=1;sc&&i<4u;i++)if(cb==sc[i])pending=1;
  for(ns_u32 i=1;ac&&i<3u;i++)if(cb==ac[i])pending=1;
  prev=node;node=R_RD(node+28u);
 }
 if(node || prev!=R_RD(R_TIMER_HEAD+8u))return NS_INVALID;
 return pending?NS_BUSY:NS_OK;
}
void nsr_report(const ns_u32 *p,ns_u32 n) {
#if NS_TARGET_BAND
 (void)p;(void)n;
#else
 int fd=R_FN(0x2c6a74c4u,int(*)(const char*,int,...))("/tmp/ns_ble_v3_result.txt",0x26,0600);if(fd<0)return;
 R_FN(0x2c34c35cu,int(*)(int,const char*,...))(fd,"NS3");
 for(ns_u32 i=0;i<n;i++)R_FN(0x2c34c35cu,int(*)(int,const char*,...))(fd," %u",p[i]);
 R_FN(0x2c34c35cu,int(*)(int,const char*,...))(fd,"\n");R_FN(0x2c6a6c2cu,int(*)(int))(fd);
#endif
}
