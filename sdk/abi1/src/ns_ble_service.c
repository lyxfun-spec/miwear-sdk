#include "ns_ble_backend.h"
struct ns1_ble {
 struct ns_ble_backend backend;
 ns_u32 tag,exiting,pending,roles,length,clock_valid,now,last_start,started,draining,drain_since;
 unsigned char frame[31];
};
#define SERVICE_TAG 0x4e533142u
static ns_u32 valid(ns1_ble *s){return s && nsp_valid(s,sizeof(*s)) && s->tag==SERVICE_TAG && nsp_ui_matches(&s->backend.ui);}
static ns_u32 load(const ns_u32 *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void store(ns_u32 *p,ns_u32 n){__atomic_store_n(p,n,__ATOMIC_RELEASE);}
void nsb_receive_copy(struct ns_ble_backend *b,ns_u32 generation,const unsigned char *p,ns_u32 n){
 if(!b->receive_enabled || n>31u || (n&&!p))return;
 ns_u32 head=load(&b->rx_head),tail=load(&b->rx_tail);
 if(head-tail>=NS1_BLE_RX_SLOTS){__atomic_fetch_add(&b->rx_dropped,1,__ATOMIC_RELAXED);return;}
 struct ns1_ble_report *r=&b->rx[head%NS1_BLE_RX_SLOTS];
 r->generation=generation;r->length=n;
 for(ns_u32 i=0;i<31u;i++)r->bytes[i]=i<n?p[i]:0;
 store(&b->rx_head,head+1u);
}
enum ns_result ns1_ble_open(ns1_ble **out){
 if(!out)return NS_INVALID;if(*out)return NS_BUSY;
 struct ns_ui_context ui;
 if(!nsp_guards() || !nsr_guards() || !nsp_ui_capture(&ui))return NS_INVALID;
 ns1_ble *s=nsp_alloc(sizeof(*s));if(!s)return NS_EXHAUSTED;
 if(!nsp_valid(s,sizeof(*s)))return NS_INVALID; /* Unknown allocation: preserve. */
 unsigned char *bytes=(void*)s;for(ns_u32 i=0;i<sizeof(*s);i++)bytes[i]=0;
 if(nsb_init(&s->backend,&ui)!=NS_OK){nsp_free(s);return NS_INVALID;}
 s->tag=SERVICE_TAG;s->backend.receive_enabled=1;*out=s;return NS_OK;
}
static ns_u32 frame_valid(const void *p,ns_u32 n){
 if(!p || !n || n>31u)return 0;
 /* Validate complete AD elements, including an optional zero-filled suffix. */
 const unsigned char *v=p;
 for(ns_u32 i=0;i<n;){ns_u32 len=v[i++];if(!len){while(i<n)if(v[i++])return 0;return 1;}if(len>n-i)return 0;i+=len;}
 return 1;
}
static void frame_copy(ns1_ble *s,const void *p,ns_u32 n){
 const unsigned char *v=p;s->length=n;for(ns_u32 i=0;i<n;i++)s->frame[i]=v[i];s->pending=1;
}
enum ns_result ns1_ble_start(ns1_ble *s,ns_u32 roles,const void *p,ns_u32 n){
 if(!valid(s)||s->exiting||!roles||(roles&~3u) || ns_state(&s->backend.controller.manager)==NS_HELD)return NS_INVALID;
 if((roles&NS_ADV)?!frame_valid(p,n):(p!=0||n!=0))return NS_INVALID;
 if(s->pending || !ns_session_quiescent(&s->backend.controller.manager))return NS_BUSY;
 s->roles=roles;frame_copy(s,p,n);return NS_OK;
}
enum ns_result ns1_ble_change(ns1_ble *s,const void *p,ns_u32 n){
 if(!valid(s)||s->exiting||!(s->roles&NS_ADV)||!frame_valid(p,n) || ns_state(&s->backend.controller.manager)==NS_HELD)return NS_INVALID;
 if(s->pending || ns_state(&s->backend.controller.manager)!=NS_ACTIVE ||
    ns_should_stop(&s->backend.controller.manager,s->backend.controller.handle))return NS_BUSY;
 frame_copy(s,p,n);return ns_controller_stop(&s->backend.controller,NS_USER);
}
enum ns_result ns1_ble_poll(ns1_ble *s,ns_u32 now){
 if(!valid(s))return NS_INVALID;
 if(s->clock_valid && now-s->now>=0x80000000u)return NS_INVALID;
 s->now=now;s->clock_valid=1;
 struct ns_ble_backend *b=&s->backend;
 nsb_heartbeat(b);ns_u32 state=ns_controller_poll(&b->controller);
 if(!ns_session_quiescent(&b->controller.manager) &&
    ns_should_stop(&b->controller.manager,b->controller.handle)){
  if(!s->draining){s->draining=1;s->drain_since=now;}
  if(now-s->drain_since>=10000u){ns_hold(&b->controller.manager,NS_PROOF_ERROR);state=NS_HELD;}
 }else s->draining=0;
 if(state==NS_HELD){s->pending=0;return NS_INVALID;}
 if(s->pending && ns_session_quiescent(&b->controller.manager)){
  if(s->started && now-s->last_start<NS1_BLE_CHANGE_MS)return NS_BUSY;
  b->custom_ad=1;b->ad_length=s->length;
  for(ns_u32 i=0;i<s->length;i++)b->ad[i]=s->frame[i];
  s->pending=0;s->started=1;s->last_start=now;
  return ns_controller_open(&b->controller,s->roles);
 }
 return NS_OK;
}
enum ns_result ns1_ble_receive(ns1_ble *s,struct ns1_ble_report *out){
 if(!valid(s)||!out)return NS_INVALID;
 struct ns_ble_backend *b=&s->backend;ns_u32 tail=load(&b->rx_tail);
 if(tail==load(&b->rx_head))return NS_BUSY;
 *out=b->rx[tail%NS1_BLE_RX_SLOTS];store(&b->rx_tail,tail+1u);return NS_OK;
}
enum ns_result ns1_ble_observe(ns1_ble *s,struct ns1_ble_snapshot *out){
 if(!valid(s)||!out)return NS_INVALID;
 struct ns_ble_backend *b=&s->backend;
 struct ns1_ble_snapshot v={ns_state(&b->controller.manager),ns_generation(&b->controller.manager),
  b->error,s->pending,load(&b->rx_head)-load(&b->rx_tail),load(&b->rx_dropped),
  ns_session_quiescent(&b->controller.manager),s->exiting};*out=v;return NS_OK;
}
enum ns_result ns1_ble_stop(ns1_ble *s){
 if(!valid(s))return NS_INVALID;s->pending=0;
 return ns_session_quiescent(&s->backend.controller.manager)?NS_OK:ns_controller_stop(&s->backend.controller,NS_USER);
}
enum ns_result ns1_ble_exit(ns1_ble *s){
 if(!valid(s))return NS_INVALID;s->exiting=1;s->pending=0;
 ns_controller_deactivate(&s->backend.controller);return NS_OK;
}
