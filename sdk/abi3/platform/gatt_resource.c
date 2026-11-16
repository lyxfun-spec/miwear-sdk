#include "ns3_resource.h"
#define W(a) (*(volatile ns_u32*)(a))
static ns_u32 enter(void){ns_u32 old,mask=128;__asm__ volatile("mrs %0,basepri":"=r"(old));__asm__ volatile("msr basepri_max,%0"::"r"(mask):"memory");return old;}
static void leave(ns_u32 old){__asm__ volatile("msr basepri,%0"::"r"(old):"memory");}
static int ptr(ns_u32 p,ns_u32 n){return nsp_valid((void*)p,n);}
static void packet(unsigned char*p){for(ns_u32 i=0;i<708;i++)p[i]=0;}
static int identity(const struct ns_ui_context*u,const struct ns_borrowed*c){return c->external&&nsr_live(u,c);}
static enum ns_result rpc(const struct ns_ui_context*u,const struct ns_borrowed*c,struct ns3_binding*b,unsigned char*p,ns_u32 cmd){
 if(!identity(u,c)){b->state=N3_UNKNOWN;return NS_INVALID;}
 b->transport_status=b->rpc_call?b->rpc_call(b->rpc_context,c,p,cmd):nsr_rpc(c,p,cmd);b->service_status=p[12];
 if(b->transport_status){b->state=N3_UNKNOWN;return NS_BUSY;}
 return b->service_status?NS_INVALID:NS_OK;
}
enum ns_result nsg3_create(const struct ns_ui_context*u,const struct ns_borrowed*c,struct ns3_binding*b,const ns_u32*cb){
 if(!b||b->token||b->node||(b->state!=N3_EMPTY&&b->state!=N3_RECLAIMED)||!identity(u,c))return NS_INVALID;
 void*context=b->rpc_context;ns_u32(*call)(void*,const struct ns_borrowed*,void*,ns_u32)=b->rpc_call;
 ns_u32(*fence)(void*,const struct ns_ui_context*,const struct ns_borrowed*,ns_u32*)=b->fence_call;
 *b=(struct ns3_binding){0};b->rpc_context=context;b->rpc_call=call;b->fence_call=fence;b->client=c->client;b->callbacks=(ns_u32)cb;
 b->token=(ns_u32)nsp_alloc(16);if(!b->token)return NS_EXHAUSTED;
 b->node=(ns_u32)nsp_alloc(12);if(!b->node){nsp_free((void*)b->token);b->token=0;return NS_EXHAUSTED;}
 if(!ptr(b->token,16)||!ptr(b->node,12)){b->state=N3_UNKNOWN;return NS_INVALID;}
 W(b->token)=c->client;W(b->token+4)=(ns_u32)cb;W(b->token+8)=0;W(b->token+12)=(ns_u32)&b->token;
 W(b->node)=W(b->node+4)=0;W(b->node+8)=b->token;b->state=N3_RESERVED;
 unsigned char p[708];packet(p);W((ns_u32)p+60)=b->token;
 enum ns_result result=rpc(u,c,b,p,0x74);
 if(result!=NS_OK){
  /* A received definite failure from the exact create dispatcher leaves no
   * service object; transport ambiguity must keep both allocations. */
  if(!b->transport_status&&b->state!=N3_UNKNOWN){nsp_free((void*)b->node);nsp_free((void*)b->token);b->node=b->token=0;b->state=N3_RECLAIMED;}
  return result;
 }
 b->remote=W((ns_u32)p+20);
 if(!b->remote||W((ns_u32)p+24)){b->state=N3_UNKNOWN;return NS_INVALID;}
 W(b->token+8)=b->remote;b->state=N3_CREATED;return NS_OK;
}
/* Validate shared list without touching unrelated data or freeing its head. */
static int list(ns_u32 h,ns_u32 needle,ns_u32*found){
 if(!ptr(h,16)||W(h+12)!=0xc34cc7du)return 0;
 ns_u32 n=W(h+4),prev=h,count=0;*found=0;
 while(n!=h&&count<128){
  if(!ptr(n,12)||W(n)!=prev)return 0;
  if(needle&&W(n+8)==needle){if(*found)return 0;*found=n;}
  prev=n;n=W(n+4);count++;
 }
 return n==h&&W(h)==prev&&W(h+8)==count;
}
static int binding(const struct ns3_binding*b){
 return ptr(b->token,16)&&ptr(b->node,12)&&W(b->token)==b->client&&W(b->token+4)==b->callbacks&&W(b->token+8)==b->remote&&W(b->token+12)==(ns_u32)&b->token&&W(b->node+8)==b->token;
}
enum ns_result nsg3_bind_ui(const struct ns_ui_context*u,const struct ns_borrowed*c,struct ns3_binding*b){
 if(!nsp_ui_matches(u)||!identity(u,c)||b->state!=N3_CREATED||b->client!=c->client||!binding(b))return NS_INVALID;
 ns_u32 fresh=0;if(!W(c->client+200)){fresh=(ns_u32)nsp_alloc(16);if(!fresh)return NS_EXHAUSTED;if(!ptr(fresh,16))return NS_INVALID;W(fresh)=W(fresh+4)=fresh;W(fresh+8)=0;W(fresh+12)=0xc34cc7d;}
 ns_u32 saved=enter(),h=W(c->client+200),found=0;enum ns_result result=NS_BUSY;
 if(!h){if(!fresh)goto done;h=fresh;W(c->client+200)=h;fresh=0;}
 if(!list(h,b->token,&found)||found||W(b->node)||W(b->node+4))goto done;
 ns_u32 tail=W(h);W(b->node)=tail;W(b->node+4)=h;W(tail+4)=b->node;W(h)=b->node;W(h+8)=W(h+8)+1;
 b->head=h;b->state=N3_LINKED;result=NS_OK;
done:leave(saved);if(fresh)nsp_free((void*)fresh);return result;
}
static enum ns_result remove_remote(const struct ns_ui_context*u,const struct ns_borrowed*c,struct ns3_binding*b){
 unsigned char p[708];packet(p);W((ns_u32)p+60)=b->remote;
 enum ns_result result=rpc(u,c,b,p,0x75);if(result==NS_OK)b->state=N3_REMOTE_GONE;return result;
}
enum ns_result nsg3_retire_remote(const struct ns_ui_context*u,const struct ns_borrowed*c,struct ns3_binding*b,struct ns3_binding*guard,ns_u32 sync[10]){
 if(b->state!=N3_LINKED||b->client!=c->client||(b->ever_attempted&&!b->terminal)||!identity(u,c))return NS_INVALID;
 /* Every path creates a fresh never-connected guard after terminal delivery.
  * Exact profile create holds200be704; response follows any preceding handler
  * and its callback writes. Guard has no address and is not locally linked. */
 guard->rpc_context=b->rpc_context;guard->rpc_call=b->rpc_call;guard->fence_call=b->fence_call;
 enum ns_result result=nsg3_create(u,c,guard,0);if(result!=NS_OK)return result;
 b->guard_fence=guard->guard_fence=1;
 result=remove_remote(u,c,b);if(result!=NS_OK)return result;
 result=remove_remote(u,c,guard);if(result!=NS_OK)return result;
 if(!(b->fence_call?b->fence_call(b->rpc_context,u,c,sync):nsr_barrier(u,c,sync)))return NS_BUSY;b->barrier_ok=guard->barrier_ok=1;return NS_OK;
}
static enum ns_result queue_locked(const struct ns_borrowed*c,ns_u32 a,ns_u32 b,ns_u32 scan){
 ns_u32 h=c->client+76,n=W(h+4),prev=h,count=0,pending=0;
 while(n!=h&&count<1024){
  if(!ptr(n,720)||W(n)!=prev||W(n+8)!=c->client)return NS_INVALID;
  ns_u32 body=n+12,code=W(body),off=0;
  if((code>=0x121&&code<=0x12c)||code==0x167||code==0x117)off=60;
  else if(code==0x168)off=340;else if(code==0x169)off=356;else if(code==0x118)off=76;else if(code==0x1060001)off=68;
  else pending=1;
  if(off){ns_u32 token=W(body+off);if(W(body+off+4)||token==a||token==b||(scan&&token==scan))pending=1;}
  prev=n;n=W(n+4);count++;
 }
 if(n!=h||W(h)!=prev)return NS_INVALID;return pending?NS_BUSY:NS_OK;
}
enum ns_result nsg3_queue_clear(const struct ns_ui_context*u,const struct ns_borrowed*c,ns_u32 a,ns_u32 b,ns_u32 scan){
 if(!nsp_ui_matches(u)||!identity(u,c))return NS_INVALID;
 ns_u32 saved=enter();enum ns_result result=queue_locked(c,a,b,scan);leave(saved);return result;
}
enum ns_result nsg3_collect_ui(const struct ns_ui_context*u,const struct ns_borrowed*c,struct ns3_binding*b,struct ns3_binding*guard,ns_u32 scan){
 if(!nsp_ui_matches(u)||!identity(u,c)||b->state!=N3_REMOTE_GONE||guard->state!=N3_REMOTE_GONE||!b->guard_fence||!guard->guard_fence||!b->barrier_ok||!guard->barrier_ok||!binding(b)||!binding(guard))return NS_INVALID;
 ns_u32 saved=enter(),h=W(c->client+200),found=0;enum ns_result result=NS_BUSY;
 if(b->head!=h||b->client!=c->client||guard->client!=c->client||b->token==guard->token||guard->head||W(guard->node)||W(guard->node+4))goto done;
 result=queue_locked(c,b->token,guard->token,scan);if(result!=NS_OK)goto done;
 if(!list(h,b->token,&found)||found!=b->node){result=NS_INVALID;goto done;}
 ns_u32 prev=W(b->node),next=W(b->node+4);W(prev+4)=next;W(next)=prev;W(h+8)=W(h+8)-1;W(b->node)=W(b->node+4)=0;
 /* Remove only proven own node; never destroy the shared framework head. */
 result=NS_OK;
done:leave(saved);
 if(result==NS_OK){ns_u32 token=b->token,node=b->node,gt=guard->token,gn=guard->node;b->token=b->node=guard->token=guard->node=0;b->state=guard->state=N3_RECLAIMED;nsp_free((void*)node);nsp_free((void*)token);nsp_free((void*)gn);nsp_free((void*)gt);}
 return result;
}
