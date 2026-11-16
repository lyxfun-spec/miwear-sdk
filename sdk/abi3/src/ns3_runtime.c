#include "ns3_runtime_private.h"
#define W(a) (*(volatile ns_u32*)(a))
#define F(a,t) ((t)((a)|1u))
static void st(ns_u32*p,ns_u32 v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
void nsg3_signal_worker(struct ns3_link*g){
 if(g->sem_ready&&!__atomic_exchange_n(&g->sem_pending,1,__ATOMIC_ACQ_REL))F(0xc3f5120,void(*)(void*))(g->wake_sem);
}
void nsg3_wait_worker(struct ns3_link*g){
 F(0xc3f513c,void(*)(void*))(g->wake_sem);st(&g->sem_pending,0);
}
static void timer_dispatch(void*t){nsg3_tick(t);}
void nsg3_enable_timer(struct ns3_link*g){
 if(!g->timer&&nsp_ui_matches(&g->ui)){g->timer=nsp_timer(timer_dispatch,g);if(!g->timer){ns_u32 z=0;__atomic_compare_exchange_n(&g->error,&z,96,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);st(&g->stop,1);nsg3_signal_worker(g);}}
}
void nsg3_wake_ui(struct ns3_link*g){
 st(&g->wake_pending,1);
 if(F(0xc3f5668,int(*)(void*))(g->runtime->async_words)){
  ns_u32 z=0;__atomic_compare_exchange_n(&g->error,&z,95,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);st(&g->stop,1);nsg3_enable_timer(g);
 }
}
static void runtime_dispatch(void*handle){
 struct ns3_runtime*r=(void*)W((ns_u32)handle);
 if(!nsp_valid(r,sizeof(*r))||r->tag!=NS3_RUNTIME_TAG||!nsp_ui_matches(&r->ui)||r->pumping)return;
 r->pumping=1;r->dispatches++;
 for(struct ns3_link*g=r->head;g;g=g->next)if(__atomic_exchange_n(&g->wake_pending,0,__ATOMIC_ACQ_REL))nsg3_pump(g);
 r->pumping=0;
}
enum ns3_result ns3_runtime_open(ns3_runtime**out){
 if(!out)return NS3_INVALID;if(*out)return NS3_BUSY;
 struct ns_ui_context ui;struct ns_borrowed b;
 if(!nsp_ui_capture(&ui))return NS3_CONTEXT;
 if(!nsp_guards()||!nsr_guards()||!nsr_borrow(&ui,&b)||!b.external)return NS3_FAILED;
 struct ns3_runtime*r=nsp_alloc(sizeof(*r));if(!r||!nsp_valid(r,sizeof(*r)))return NS3_FAILED;
 for(ns_u32 i=0;i<sizeof(*r)/4;i++)((ns_u32*)r)[i]=0;r->tag=NS3_RUNTIME_TAG;r->ui=ui;r->client=b;*out=r;
 if(F(0xc3f455c,int(*)(ns_u32,void*,void(*)(void*)))(b.external,r->async_words,runtime_dispatch))return NS3_HELD;
 W((ns_u32)r->async_words)=(ns_u32)r;r->ready=1;return NS3_OK;
}
enum ns3_result ns3_runtime_snapshot_get(ns3_runtime*r,struct ns3_runtime_snapshot*s){
 if(!r||!nsp_valid(r,sizeof(*r))||r->tag!=NS3_RUNTIME_TAG||!s||s->size!=sizeof(*s))return NS3_INVALID;
 if(!nsp_ui_matches(&r->ui))return NS3_CONTEXT;
 s->abi=3;s->sessions_opened=r->opened;s->sessions_destroyed=r->destroyed;s->sessions_live=r->live;s->session_bytes_live=r->bytes;s->runtime_bytes=sizeof(*r);s->dispatches=r->dispatches;return NS3_OK;
}
