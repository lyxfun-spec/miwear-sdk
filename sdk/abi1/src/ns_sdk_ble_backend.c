#include "ns_ble_internal.h"
#define B_WORD(p) (*(volatile ns_u32*)(p))
static ns_u32 bl(const ns_u32 *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void bs(ns_u32 *p,ns_u32 v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
static void bi(ns_u32 *p){__atomic_fetch_add(p,1,__ATOMIC_ACQ_REL);}
static struct ns_ble_owner *bowner(void *token,ns_u32 adv) {
 ns_u32 t=(ns_u32)token,off=adv?4u:(nsr_target()==NS_BAND11?12u:4u);
 if(!nsp_valid(token,off+4u))return 0;
 ns_u32 table=B_WORD(t+off),offset=adv?__builtin_offsetof(struct ns_ble_owner,adv_cb):__builtin_offsetof(struct ns_ble_owner,scan_cb);
 struct ns_ble_owner *o=(void*)(table-offset);
 if(!nsp_valid(o,sizeof(*o)) || o->tag!=NSB_TAG || table!=(ns_u32)(adv?o->adv_cb:o->scan_cb) ||
    (adv?o->adv.token:o->scan.token)!=t || !nsp_valid(o->backend,sizeof(*o->backend)))return 0;
 return o;
}
static ns_u32 benter(struct ns_ble_owner *o) {
 if(ns_callback_enter(&o->backend->controller.manager,o->handle)!=NS_OK)return 0;
 bi(&o->active);bi(&o->callbacks);ns_u32 tid=0,old=0;
 if(!nsr_callback_context(&o->backend->ui,&o->client,&tid)){bs(&o->error,31);return 1;}
 __atomic_compare_exchange_n(&o->callback_tid,&old,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 if(old && old!=tid){bs(&o->callback_mixed,1);bs(&o->error,32);}
 return 1;
}
static void bleave(struct ns_ble_owner *o) {
 __atomic_fetch_sub(&o->active,1,__ATOMIC_ACQ_REL);
 (void)ns_callback_leave(&o->backend->controller.manager,o->handle);
}
static void bscan_result(void *token,unsigned char *report) {
 struct ns_ble_owner *o=bowner(token,0);if(!o || !benter(o))return;
 if(!bl(&o->error) && report) {
  ns_u32 base=(ns_u32)report,shift=base&3u;
  struct ns_ble_layout l;unsigned char copy[31];ns_u32 n=0;
  if(ns_ble_layout(nsr_target(),&l)==NS_OK && nsp_valid((void*)(base&~3u),l.result_data+shift)) {
   ns_u32 len=report[10];
   if(len<=31u && nsp_valid((void*)(base&~3u),l.result_data+len+shift) &&
      ns_ble_copy_report(nsr_target(),report,l.result_data+len,copy,31,&n)==NS_OK){
    bi(&o->reports);nsb_receive_copy(o->backend,o->handle.generation,copy,n);
   }
   /* No address, RSSI or borrowed payload pointer is retained. */
  }
 }
 bleave(o);
}
static void bscan_start(void *token,unsigned char status) {
 struct ns_ble_owner *o=bowner(token,0);if(!o || !benter(o))return;
 if(!bl(&o->error)){bi(&o->scan.started);bs(&o->scan.status,status);}bleave(o);
}
static void bscan_stop(void *token) {
 struct ns_ble_owner *o=bowner(token,0);if(!o || !benter(o))return;
 if(!bl(&o->error))bi(&o->scan.stopped);bleave(o);
}
static void badv_start(void *token,unsigned char id,unsigned char status) {
 struct ns_ble_owner *o=bowner(token,1);if(!o || !benter(o))return;
 if(!bl(&o->error)){bs(&o->adv.id,id);bi(&o->adv.started);bs(&o->adv.status,status);}bleave(o);
}
static void badv_stop(void *token,unsigned char id) {
 struct ns_ble_owner *o=bowner(token,1);if(!o || !benter(o))return;
 if(!bl(&o->error)){if(bl(&o->adv.id)!=id)bs(&o->error,33);bi(&o->adv.stopped);}bleave(o);
}
static ns_u32 bpause(struct ns_ble_owner *o) {if(nsp_pause()){bs(&o->error,34);return 0;}return 1;}
static ns_u32 bstart_role(struct ns_ble_owner *o,ns_u32 adv) {
 struct ns_radio_role *r=adv?&o->adv:&o->scan;struct ns_ble_layout l;
 if(ns_should_stop(&o->backend->controller.manager,o->handle))return 1;
 if(!nsr_live(&o->backend->ui,&o->client)){bs(&o->error,10);return 0;}
 ns_ble_layout(nsr_target(),&l);ns_u32 size=adv?l.adv_token_bytes:l.scan_token_bytes;
 void *token=nsp_alloc(size);r->token=(ns_u32)token;
 if(!token || !nsp_valid(token,size)){bs(&o->error,11);return 0;}
 unsigned char *v=token;for(ns_u32 i=0;i<size;i++)v[i]=0;
 ns_u32 off=adv?4u:(nsr_target()==NS_BAND11?12u:4u);
 B_WORD((ns_u32)token+off)=(ns_u32)(adv?o->adv_cb:o->scan_cb);
 if(adv && nsr_target()==NS_S4)B_WORD(token)=o->client.client;
 ns_u32 packet[177];
 enum ns_result encoded=adv?ns_ble_adv(nsr_target(),(void*)packet,l.packet_bytes,r->token,o->ad,o->ad_length):
     ns_ble_scan(nsr_target(),(void*)packet,l.packet_bytes,r->token);
 if(encoded!=NS_OK){bs(&o->error,12);return 0;}
 r->attempted=1;__atomic_thread_fence(__ATOMIC_RELEASE);
 r->start_rpc=nsr_rpc(&o->client,packet,adv?l.adv_start:l.scan_start);
 (void)ns_ble_remote(nsr_target(),(void*)packet,l.packet_bytes,&r->remote);
 /* The decoder may already have freed token; only the saved address is used. */
 if(r->start_rpc || !(r->remote.lo|r->remote.hi)){bs(&o->error,13);return 0;}
 for(ns_u32 i=0;i<500u && !bl(&r->started);i++)if(!bpause(o))break;
 if(!bl(&r->started) || bl(&r->status)!=0){bs(&o->error,14);return 0;}
 return 1;
}
static void bstop_role(struct ns_ble_owner *o,ns_u32 adv) {
 struct ns_radio_role *r=adv?&o->adv:&o->scan;struct ns_ble_layout l;
 if(!r->attempted || !(r->remote.lo|r->remote.hi) || bl(&r->stopped))return;
 ns_u32 status=bl(&r->status);if(status && status!=255u)return;
 if(!nsr_live(&o->backend->ui,&o->client)){bs(&o->error,15);return;}
 ns_ble_layout(nsr_target(),&l);ns_u32 packet[177];
 if(ns_ble_stop(nsr_target(),(void*)packet,l.packet_bytes,r->remote)!=NS_OK){bs(&o->error,16);return;}
 r->stop_rpc=nsr_rpc(&o->client,packet,adv?l.adv_stop:l.scan_stop);
 if(r->stop_rpc){bs(&o->error,17);return;}
 for(ns_u32 i=0;i<500u && !bl(&r->stopped);i++)if(!bpause(o))break;
 if(!bl(&r->stopped))bs(&o->error,18);
}
static ns_u32 bterminal(const struct ns_radio_role *r) {
 return !r->attempted?(!r->token):bl(&r->stopped)==1u;
}
void *nsb_worker(void *arg) {
 struct ns_ble_owner *o=arg;if(!nsp_valid(o,sizeof(*o)) || o->tag!=NSB_TAG)return 0;
 struct ns_ble_backend *b=o->backend;if(!nsp_valid(b,sizeof(*b)))return 0;
 if(!nsp_worker_identity(&b->ui,&o->actual_tid) || !nsr_borrow(&b->ui,&o->client)){bs(&o->error,1);goto done;}
 if((o->roles&NS_SCAN) && !bstart_role(o,0))goto stop;
 if((o->roles&NS_ADV) && !bstart_role(o,1))goto stop;
 if(!ns_should_stop(&b->controller.manager,o->handle)) {
  (void)ns_worker_ready(&b->controller.manager,o->handle);
  ns_u32 last=bl(&b->heartbeat),stale=0;
  for(ns_u32 i=0;!o->limit || i<o->limit;i++) {
   if(ns_should_stop(&b->controller.manager,o->handle))break;
   if(!bpause(o))break;
   ns_u32 now=bl(&b->heartbeat);stale=now==last?stale+1u:0u;last=now;
   if(stale>=200u){bs(&o->error,19);break;}
  }
 }
stop:
 bstop_role(o,0);bstop_role(o,1);
 if(!bterminal(&o->scan) || !bterminal(&o->adv))bs(&o->error,20);
 if(!bl(&o->error)) {
  if(nsr_barrier(&b->ui,&o->client,o->sync))bs(&o->barrier,1);
  else bs(&o->error,21);
 }
done:
 bs(&o->done,1);(void)ns_worker_done(&b->controller.manager,o->handle);
 return (void*)(0x53a00000u|o->handle.generation);
}
static enum ns_result bb_start(void *arg,struct ns_manager *m,struct ns_handle h,ns_u32 roles) {
 struct ns_ble_backend *b=arg;
 if(!roles || roles&~3u || b->owner || m!=&b->controller.manager || !nsp_ui_matches(&b->ui))return NS_INVALID;
 struct ns_ble_owner *o=nsp_alloc(sizeof(*o));b->owner=o;
 if(!o || !nsp_valid(o,sizeof(*o))){b->error=40;return NS_INVALID;}
 unsigned char *p=(void*)o;for(ns_u32 i=0;i<sizeof(*o);i++)p[i]=0;
 o->tag=NSB_TAG;o->backend=b;o->handle=h;o->roles=roles;o->limit=b->limit;o->scan.status=o->adv.status=255;
 volatile ns_u32 *r=o->scan_cb;r[0]=16;r[1]=(ns_u32)bscan_result;r[2]=(ns_u32)bscan_start;r[3]=(ns_u32)bscan_stop;
 r=o->adv_cb;r[0]=12;r[1]=(ns_u32)badv_start;r[2]=(ns_u32)badv_stop;
 o->ad[0]=2;o->ad[1]=1;o->ad[2]=6;o->ad[3]=8;o->ad[4]=9;o->ad[5]='N';o->ad[6]='S';o->ad[7]='5';o->ad[8]='-';
 o->ad[9]=nsr_target()==NS_BAND11?'B':'S';o->ad[10]=nsr_target()==NS_BAND11?'1':'4';o->ad[11]=nsr_target()==NS_BAND11?'1':'S';
 o->ad_length=12;
 if(b->custom_ad){o->ad_length=b->ad_length;for(ns_u32 i=0;i<o->ad_length;i++)o->ad[i]=b->ad[i];}
 __atomic_thread_fence(__ATOMIC_RELEASE);
 if(nsp_spawn(&o->tid,nsb_worker,o)){b->error=41;return NS_INVALID;}b->created++;
 if(!o->tid || o->tid==b->ui.tid){b->error=42;return NS_INVALID;}return NS_OK;
}
static void bb_stop(void *arg,struct ns_handle h) {
 struct ns_ble_backend *b=arg;if(h.generation!=ns_generation(&b->controller.manager))b->error=43;
}
static enum ns_result bb_inspect(void *arg,struct ns_handle h,struct ns_evidence *e) {
 struct ns_ble_backend *b=arg;struct ns_ble_owner *o=b->owner;
 if(!nsp_ui_matches(&b->ui) || !nsp_valid(o,sizeof(*o)) || o->tag!=NSB_TAG || o->handle.generation!=h.generation)return NS_INVALID;
 if(bl(&o->error) || b->error){b->error=bl(&o->error)?bl(&o->error):b->error;return NS_INVALID;}
 e->generation=h.generation;
 if((o->roles&NS_SCAN) && bterminal(&o->scan))e->terminal_roles|=NS_SCAN;
 if((o->roles&NS_ADV) && bterminal(&o->adv))e->terminal_roles|=NS_ADV;
 e->internal_barrier=bl(&o->barrier);
 if(!bl(&o->done)){b->overlaps++;return NS_OK;}
 if(!e->internal_barrier || e->terminal_roles!=o->roles)return NS_BUSY;
 if(!o->joined) {
  enum ns_result exited=nsp_exit_proof(&b->ui,o->tid,0x53a00000u|h.generation);
  if(exited==NS_INVALID){b->error=44;return NS_INVALID;}
  if(exited==NS_OK){e->worker_exited=1;if(!o->proof){o->proof=1;b->exit_proofs++;}}
  return NS_OK;
 }
 e->worker_exited=1;
 if(!nsr_live(&b->ui,&o->client) || bl(&o->callback_mixed)){b->error=45;return NS_INVALID;}
 if(bl(&o->active))return NS_OK;
 enum ns_result queue=nsr_queue_clear(&b->ui,&o->client,o->scan.token,o->adv.token);
 enum ns_result timers=nsr_timers_clear(o,sizeof(*o),o->scan_cb,o->adv_cb,o->scan.token,o->adv.token);
 if(queue==NS_INVALID || timers==NS_INVALID){b->error=46;return NS_INVALID;}
 e->external_ui=1;e->own_queue_clear=queue==NS_OK;e->own_timers_clear=timers==NS_OK;
 if(e->own_queue_clear && e->own_timers_clear)b->quiet_checks++;
 return NS_OK;
}
static enum ns_result bb_join(void *arg,struct ns_handle h) {
 struct ns_ble_backend *b=arg;struct ns_ble_owner *o=b->owner;void *value=0;
 if(!nsp_ui_matches(&b->ui) || !nsp_valid(o,sizeof(*o)) || !o->proof || o->joined || o->handle.generation!=h.generation)return NS_INVALID;
 if(nsp_join(o->tid,&value)){b->error=47;return NS_INVALID;}o->joined=1;b->joined++;
 if((ns_u32)value!=(0x53a00000u|h.generation) || bl(&o->error) || o->actual_tid!=o->tid){b->error=48;return NS_INVALID;}
 return NS_OK;
}
static enum ns_result bb_release(void *arg,struct ns_handle h) {
 struct ns_ble_backend *b=arg;struct ns_ble_owner *o=b->owner;
 if(!nsp_ui_matches(&b->ui) || !nsp_valid(o,sizeof(*o)) || o->tag!=NSB_TAG || !o->joined || !bl(&o->barrier) ||
    bl(&o->active) || bl(&o->error) || b->error || o->handle.generation!=h.generation)return NS_INVALID;
 if(o->scan.attempted && (bl(&o->scan.started)!=1 || bl(&o->scan.status) || bl(&o->scan.stopped)!=1))return NS_INVALID;
 if(o->adv.attempted && (bl(&o->adv.started)!=1 || bl(&o->adv.status) || bl(&o->adv.stopped)!=1))return NS_INVALID;
 b->scan_start+=bl(&o->scan.started);b->scan_stop+=bl(&o->scan.stopped);b->adv_start+=bl(&o->adv.started);b->adv_stop+=bl(&o->adv.stopped);
 b->barriers++;b->callbacks+=bl(&o->callbacks);b->reports+=bl(&o->reports);b->last_tid=o->tid;b->last_mode=o->client.external?1:2;
 /* Token ownership belongs to firmware decoders; never touch the old token.
  * Barrier + joined worker + later UI/queue/timer proof authorizes owner free. */
 o->tag=0;nsp_free(o);b->owner=0;b->released++;return NS_OK;
}
enum ns_result nsb_init(struct ns_ble_backend *b,const struct ns_ui_context *ui) {
 if(!nsp_valid(b,sizeof(*b)) || !nsp_ui_matches(ui) || !nsr_guards())return NS_INVALID;
 b->ui=*ui;volatile struct ns_backend *op=&b->ops;
 op->start=bb_start;op->request_stop=bb_stop;op->inspect=bb_inspect;op->join=bb_join;op->release=bb_release;
 return ns_controller_init(&b->controller,&b->ops,b);
}
void nsb_heartbeat(struct ns_ble_backend *b){bi(&b->heartbeat);}
