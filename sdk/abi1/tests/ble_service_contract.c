#include "ns_ble_backend.h"
#include <stdio.h>
static unsigned checks,failure,ui_ok=1,guards=1,ready=1,quiet=1,exited=1,joins,frees,starts;
#define CK(x) do{checks++;if(!(x)){failure=__LINE__;goto end;}}while(0)
static struct ns_ui_context captured={1,2,3,4,5};
static ns_u32 arena[512];
ns_u32 nsp_guards(void){return guards;}
ns_u32 nsr_guards(void){return guards;}
void *nsp_alloc(ns_u32 n){return n<=sizeof(arena)?arena:0;}
void nsp_free(void *p){(void)p;}
ns_u32 nsp_valid(const void *p,ns_u32 n){(void)n;return p!=0;}
ns_u32 nsp_ui_capture(struct ns_ui_context *out){*out=captured;return ui_ok;}
ns_u32 nsp_ui_matches(const struct ns_ui_context *p){return p->tid==3&&ui_ok;}
static enum ns_result start(void *p,struct ns_manager *m,struct ns_handle h,ns_u32 roles){
 struct ns_ble_backend *b=p;(void)roles;b->owner=(void*)1;starts++;
 if(ready)(void)ns_worker_ready(m,h);return NS_OK;
}
static void stop(void *p,struct ns_handle h){(void)p;(void)h;}
static enum ns_result inspect(void *p,struct ns_handle h,struct ns_evidence *e){
 struct ns_ble_backend *b=p;e->generation=h.generation;
 if(ns_should_stop(&b->controller.manager,h)){
  (void)ns_worker_done(&b->controller.manager,h);e->terminal_roles=b->controller.manager.roles;
  e->internal_barrier=1;e->worker_exited=exited;e->external_ui=1;
  e->own_queue_clear=quiet;e->own_timers_clear=quiet;
 }return NS_OK;
}
static enum ns_result join(void *p,struct ns_handle h){(void)p;(void)h;joins++;return NS_OK;}
static enum ns_result release(void *p,struct ns_handle h){struct ns_ble_backend *b=p;(void)h;frees++;b->owner=0;return NS_OK;}
enum ns_result nsb_init(struct ns_ble_backend *b,const struct ns_ui_context *ctx){
 b->ui=*ctx;b->ops=(struct ns_backend){start,stop,inspect,join,release};
 return ns_controller_init(&b->controller,&b->ops,b);
}
void nsb_heartbeat(struct ns_ble_backend *b){b->heartbeat++;}
/* The mock replaces firmware scheduling only; production service/controller,
 * ring, packet encoder and report parser execute as ARM instructions. */
#include "../src/ns_ble_service.c"
#include "ns_ble_packet.h"
int main(void){
 ns1_ble *s=0;struct ns1_ble_snapshot snap;struct ns1_ble_report r;
 unsigned char ad[31]={30,0xff},original[31];
 for(unsigned i=2;i<31;i++)ad[i]=(unsigned char)i;
 for(unsigned i=0;i<31;i++)original[i]=ad[i];
 guards=0;CK(ns1_ble_open(&s)==NS_INVALID&&!s);guards=1;
 CK(ns1_ble_open(&s)==NS_OK);CK(ns1_ble_open(&s)==NS_BUSY);
 {unsigned char single=0;CK(ns1_ble_start(s,NS_ADV,&single,1)==NS_OK);CK(ns1_ble_stop(s)==NS_OK);}
 {unsigned char invalid[]={0,1};CK(ns1_ble_start(s,NS_ADV,invalid,2)==NS_INVALID);}
 CK(ns1_ble_start(s,NS_SCAN,ad,31)==NS_INVALID);
 CK(ns1_ble_start(s,0,ad,31)==NS_INVALID);
 CK(ns1_ble_start(s,NS_ADV,ad,32)==NS_INVALID);
 CK(ns1_ble_start(s,NS_ADV,0,1)==NS_INVALID);
 CK(ns1_ble_start(s,NS_ADV,ad,0)==NS_INVALID);
 ad[0]=31;CK(ns1_ble_start(s,NS_ADV,ad,31)==NS_INVALID);ad[0]=30;
 CK(ns1_ble_start(s,3,ad,31)==NS_OK);ad[2]=99;
 CK(ns1_ble_poll(s,0xfffffff0u)==NS_OK);CK(starts==1&&s->backend.ad[2]==2);
 CK(ns1_ble_poll(s,0xfffffff1u)==NS_OK);
 CK(ns1_ble_start(s,3,ad,31)==NS_BUSY);
 for(unsigned i=0;i<9;i++)nsb_receive_copy(&s->backend,1,original,31);
 original[2]=88;
 CK(ns1_ble_observe(s,&snap)==NS_OK&&snap.queued_rx==8&&snap.dropped_rx==1);
 for(unsigned i=0;i<8;i++){CK(ns1_ble_receive(s,&r)==NS_OK&&r.bytes[2]==2&&r.generation==1&&r.length==31);}
 CK(ns1_ble_receive(s,&r)==NS_BUSY);
 /* Counter wrap and independent caller copies. */
 s->backend.rx_head=s->backend.rx_tail=0xfffffffeu;
 for(unsigned i=0;i<3;i++)nsb_receive_copy(&s->backend,2,ad,1);
 for(unsigned i=0;i<3;i++)CK(ns1_ble_receive(s,&r)==NS_OK&&r.length==1&&r.bytes[1]==0);
 CK(ns1_ble_receive(s,&r)==NS_BUSY);
 ui_ok=0;CK(ns1_ble_poll(s,0xfffffff2u)==NS_INVALID);CK(ns1_ble_exit(s)==NS_INVALID);
 CK(ns1_ble_receive(s,&r)==NS_INVALID);ui_ok=1;
 CK(ns1_ble_change(s,ad,31)==NS_OK);ad[2]=66;
 CK(ns1_ble_change(s,ad,31)==NS_BUSY);
 exited=0;CK(ns1_ble_poll(s,0x10)==NS_OK&&joins==0&&frees==0);
 exited=1;quiet=0;CK(ns1_ble_poll(s,0x11)==NS_OK&&joins==1&&frees==0);
 CK(ns1_ble_poll(s,0x12)==NS_OK&&frees==0);
 quiet=1;CK(ns1_ble_poll(s,0x13)==NS_BUSY&&frees==1&&starts==1);
 CK(ns1_ble_poll(s,0x3d8)==NS_OK&&starts==2&&s->backend.ad[2]==99);
 CK(ns1_ble_poll(s,0x3d7)==NS_INVALID); /* backward clock */
 CK(ns1_ble_poll(s,0x3d9)==NS_OK);
 CK(ns1_ble_stop(s)==NS_OK);for(unsigned i=0;i<4;i++)(void)ns1_ble_poll(s,0x400+i);
 CK(ns1_ble_observe(s,&snap)==NS_OK&&snap.quiet&&frees==2);
 CK(ns1_ble_start(s,NS_SCAN,0,0)==NS_OK);
 CK(ns1_ble_stop(s)==NS_OK);CK(ns1_ble_poll(s,0x800)==NS_OK&&starts==2); /* cancel intent */
 CK(ns1_ble_start(s,NS_SCAN,0,0)==NS_OK);CK(ns1_ble_poll(s,0x801)==NS_OK&&starts==3);
 CK(ns1_ble_exit(s)==NS_OK);quiet=0;
 for(unsigned i=0;i<4;i++)(void)ns1_ble_poll(s,0x802+i);
 CK(frees==2);CK(ns1_ble_start(s,NS_SCAN,0,0)==NS_INVALID);
 CK(ns1_ble_poll(s,0x4000)==NS_INVALID);
 quiet=1;CK(ns1_ble_poll(s,0x4001)==NS_INVALID);
 CK(ns1_ble_observe(s,&snap)==NS_OK&&!snap.quiet&&snap.state==NS_HELD&&snap.exiting&&frees==2);
 /* Both firmware encodings: exact 31 bytes, >31 rejection, report copy. */
 for(unsigned t=0;t<2;t++){
  enum ns_target target=t?NS_S4:NS_BAND11;struct ns_ble_layout l;unsigned char packet[708],copy[31];unsigned n=0;
  CK(ns_ble_layout(target,&l)==NS_OK);
  CK(ns_ble_adv(target,packet,l.packet_bytes,4,ad,31)==NS_OK);
  CK(packet[(t?76:100)+30]==ad[30]);
  CK(ns_ble_adv(target,packet,l.packet_bytes,4,ad,32)==NS_INVALID);
  packet[10]=31;for(unsigned i=0;i<31;i++)packet[l.result_data+i]=(unsigned char)i;
  CK(ns_ble_copy_report(target,packet,l.result_data+31,copy,31,&n)==NS_OK&&n==31&&copy[30]==30);
  CK(ns_ble_copy_report(target,packet,l.result_data+30,copy,31,&n)==NS_INVALID);
  packet[10]=32;CK(ns_ble_copy_report(target,packet,l.result_data+32,copy,31,&n)==NS_INVALID);
 }
end:
 printf("",1,checks,failure);return failure?1:0;
}
