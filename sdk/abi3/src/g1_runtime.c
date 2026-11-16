#include "ns3_runtime_private.h"
#define WORD(a) (*(volatile ns_u32*)(a))
#define FN(a,t) ((t)((a)|1u))
static ns_u32 ld(ns_u32*p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void st(ns_u32*p,ns_u32 v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
static void inc(ns_u32*p){__atomic_fetch_add(p,1,__ATOMIC_ACQ_REL);}
static void fail(struct g1*g,ns_u32 e){ns_u32 z=0;__atomic_compare_exchange_n(&g->error,&z,e,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
static struct g1 *owner(void*t,int scan){
 if(!nsp_valid(t,16))return 0;
 ns_u32 table=WORD((ns_u32)t+(scan?12:4));
 for(ns_u32 i=0;i<(scan?1u:2u);i++){
  struct g1*g=(void*)(table-(scan?__builtin_offsetof(struct g1,scan_cb):__builtin_offsetof(struct g1,gatt_cb)+i*52));
  if(!nsp_valid(g,sizeof(*g))||g->tag!=G1_TAG)continue;
  if(scan?(g->scan_token!=(ns_u32)t):(g->tokens[i]!=(ns_u32)t))continue;
  if(!scan&&g->slot_generation[i]!=ld(&g->round)){inc(&g->late);return 0;}
  inc(&g->active);inc(&g->callbacks);
  ns_u32 tid=0;if(!nsr_callback_context(&g->ui,&g->client,&tid)){fail(g,31);__atomic_fetch_sub(&g->active,1,__ATOMIC_ACQ_REL);return 0;}
  else {ns_u32 z=0;__atomic_compare_exchange_n(&g->callback_tid,&z,tid,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);if(z&&z!=tid)fail(g,32);}
  return g;
 }
 return 0;
}
static int uuid(const unsigned char*p,const unsigned char*u){ns_u32 a=1,b=1;for(ns_u32 i=0;i<16;i++){if(p[i]!=u[i])a=0;if(p[i]!=u[15-i])b=0;}return a||b;}
static int advertisement(struct g1*g,const unsigned char*p,ns_u32 n){
 if(n>31)return 0;for(ns_u32 i=0;i<n;){ns_u32 k=p[i];if(!k)break;if(k>n-i-1)return 0;
  if((p[i+1]==6||p[i+1]==7)&&k>=17)for(ns_u32 j=2;j+15<=k;j+=16)if(uuid(p+i+j,g->config.service_uuid))return 1;
  i+=k+1;
 }return 0;
}
static void leave(struct g1*g){__atomic_fetch_sub(&g->active,1,__ATOMIC_ACQ_REL);nsg3_signal_worker(g);nsg3_wake_ui(g);}
static int canceled(struct g1*g){return ld(&g->stop)||ld(&g->disconnect_request);}
static int span(const void*p,ns_u32 n){ns_u32 a=(ns_u32)p;return p&&nsp_valid((void*)(a&~3u),n+(a&3u));}
static void scan_result(void*t,unsigned char*p){struct g1*g=owner(t,1);if(!g)return;
 if(!ld(&g->stop)&&!ld(&g->peer_found)&&span(p,12)&&p[10]<=31&&span(p,12+p[10])&&advertisement(g,p+12,p[10])){
  for(ns_u32 i=0;i<6;i++)g->peer[i]=p[i];g->peer[6]=p[8];st(&g->peer_found,1);
 }leave(g);
}
static void scan_start(void*t,unsigned char status){struct g1*g=owner(t,1);if(!g)return;if(status)fail(g,40+status);inc(&g->scan_started);leave(g);}
static void scan_stop(void*t){struct g1*g=owner(t,1);if(!g)return;inc(&g->scan_stopped);leave(g);}
static void on_connect(void*t,unsigned char*a){(void)a;struct g1*g=owner(t,0);if(!g)return;inc(&g->connected);leave(g);}
static void on_disconnect(void*t,unsigned char*a){(void)a;struct g1*g=owner(t,0);if(!g)return;inc(&g->disconnected);g->bindings[(g->round-1)&1].terminal=1;if(ld(&g->phase)!=8&&!ld(&g->stop)&&!ld(&g->disconnect_request)){inc(&g->unexpected_disconnect);fail(g,51);}leave(g);}
static void on_discover(void*t,unsigned char status,unsigned char*u,unsigned short start,unsigned short end){
 struct g1*g=owner(t,0);if(!g)return;if(canceled(g)){inc(&g->late);leave(g);return;}g->gatt_status=status;
 if(status)fail(g,52);else if(!u||(span(u,20)&&!u[0]))inc(&g->discovered);
 else if(span(u,20)&&u[0]==16&&uuid(u+4,g->config.service_uuid)&&start&&end>=start&&end-start<128){g->service_start=start;g->service_end=end;}
 leave(g);
}
static void on_read(void*t,unsigned char status,unsigned short h,unsigned char*p,unsigned short n){
 struct g1*g=owner(t,0);if(!g)return;
 g->read_last_status=status;g->read_last_handle=h;g->read_last_length=n;
 /* A terminal/canceled ATT response still participates in callback draining.
  * It does not validate/dereference canceled data or reopen an application op. */
 if(canceled(g)){inc(&g->read_cancelled);inc(&g->late);leave(g);return;}
 g->gatt_status=status;
 ns_u32 why=(status?1u:0u)|(h!=g->characteristic?2u:0u)|(!span(p,n)?4u:0u)|(n>20?8u:0u)|(ld(&g->read_pending)?16u:0u);
 if(why){g->read_bad_reason=why;fail(g,53);}
 else {for(ns_u32 i=0;i<n;i++)g->read_bytes[i]=p[i];g->read_size=n;inc(&g->read);st(&g->read_pending,1);}leave(g);
}
static void on_write(void*t,unsigned char status,unsigned short h){struct g1*g=owner(t,0);if(!g)return;if(canceled(g)){inc(&g->late);leave(g);return;}g->gatt_status=status;if(status||h!=g->characteristic)fail(g,54);else inc(&g->written);leave(g);}
static void on_sub(void*t,unsigned char status,unsigned short h,unsigned char enable){struct g1*g=owner(t,0);if(!g)return;if(canceled(g)){inc(&g->late);leave(g);return;}g->gatt_status=status;if(status||h!=g->characteristic||!enable)fail(g,55);else if(g->sub_generation!=ld(&g->round)){g->sub_generation=ld(&g->round);inc(&g->subscribed);}else inc(&g->late);leave(g);}
static void on_notify(void*t,unsigned short h,unsigned char*p,unsigned short n){struct g1*g=owner(t,0);if(!g)return;
 if(ld(&g->stop)||ld(&g->disconnect_request)||h!=g->characteristic)inc(&g->late);
 else if(!span(p,n)||n>20||g->rx_write-g->rx_read>=8)fail(g,86);
 else {ns_u32 at=g->rx_write&7;for(ns_u32 i=0;i<n;i++)g->rx_queue[at].bytes[i]=p[i];g->rx_queue[at].length=n;g->rx_queue[at].generation=g->round;inc(&g->notified);inc(&g->rx_write);}leave(g);
}
static void on_mtu(void*t,unsigned char status,ns_u32 mtu){
 struct g1*g=owner(t,0);if(!g)return;
 inc(&g->mtu_events);g->mtu_last_status=status;g->mtu_last_value=mtu;
 if(!mtu)inc(&g->mtu_zero);
 /* This exact firmware's automatic callback transports ATT payload bytes,
  * not ATT MTU: ROM subtracts3 before dispatch (23->20, 517->514).
  * Do not subtract3 again or apply the generic minimum ATT MTU of23 here.
  * Other targets/explicit MTU request paths need separate contracts.
  * G2 still sends only8 bytes under a fixed conservative20-byte cap. */
 ns_u32 invalid=!status&&mtu&&(mtu<20||mtu>514);
 if(status||!mtu||invalid){
  if(!ld(&g->mtu_warnings)&&!ld(&g->mtu_invalid)){
   g->mtu_first_bad_status=status;g->mtu_first_bad_value=mtu;
   g->mtu_first_bad_round=ld(&g->round);g->mtu_first_bad_phase=ld(&g->phase);
  }
  if(invalid){inc(&g->mtu_invalid);fail(g,56);}else inc(&g->mtu_warnings);
 }else st(&g->payload_limit,mtu);
 leave(g);
}
static int wait_for(struct g1*g,ns_u32*p,ns_u32 n,ns_u32 limit,int cleanup){
 if(p==&g->connected)limit=(g->config.connect_timeout_ms+9)/10;
 st(&g->wait_deadline,ld(&g->ticks)+(limit+9)/10);nsg3_wake_ui(g);
 for(;;){
  if(ld(p)>=n){st(&g->wait_deadline,0);return 1;}
  if(!cleanup&&(ld(&g->stop)||ld(&g->error)||ld(&g->disconnect_request))){st(&g->wait_deadline,0);return 0;}
  if(ld(&g->ticks)>=ld(&g->wait_deadline)){st(&g->wait_deadline,0);fail(g,62);return 0;}
  nsg3_wait_worker(g);
 }
}
static ns_u32 managed_rpc(void*arg,const struct ns_borrowed*c,void*p,ns_u32 cmd){
 struct g1*g=arg;g->rpc_command=cmd;g->rpc_deadline=ld(&g->ticks)+(g->config.operation_timeout_ms+99)/100;st(&g->rpc_active,1);nsg3_wake_ui(g);
 ns_u32 r=nsr_rpc(c,p,cmd);g->rpc_transport=r;g->rpc_service=((unsigned char*)p)[12];st(&g->rpc_active,0);nsg3_wake_ui(g);return r;
}
static ns_u32 managed_fence(void*arg,const struct ns_ui_context*u,const struct ns_borrowed*c,ns_u32*sync){
 struct g1*g=arg;g->rpc_command=0x10000000;g->rpc_deadline=ld(&g->ticks)+(g->config.operation_timeout_ms+99)/100;st(&g->rpc_active,1);nsg3_wake_ui(g);
 ns_u32 r=nsr_barrier(u,c,sync);st(&g->rpc_active,0);nsg3_wake_ui(g);return r;
}
static ns_u32 operation(struct g1*g,unsigned char*p,ns_u32 cmd){
 ns_u32 r=managed_rpc(g,&g->client,p,cmd);g->rpc_status=r?r:p[12];return g->rpc_status;
}
static int live(struct g1*g){if(!nsr_live(&g->ui,&g->client)){fail(g,63);return 0;}return 1;}
/* Exact ROM returns0x33 when a valid handle has no cached element.
 * A service interval is not a dense list of cached characteristic handles.
 * Only that result may be skipped, and only for this bounded lookup. */
static int attribute(struct g1*g,unsigned char*p,ns_u32 h){
 if(!live(g))return -1;
 inc(&g->attr_queries);ns_u32 r=managed_rpc(g,&g->client,p,0x79),s=p[12];g->rpc_status=r?r:s;
 if(!r&&s==0x33){if(!g->attr_misses)g->attr_first_miss=h;inc(&g->attr_misses);return 0;}
 if(r||s){
  g->attr_first_transport=r;g->attr_first_status=s;g->attr_first_handle=h;
  g->attr_first_round=ld(&g->round);g->attr_first_phase=ld(&g->phase);
  fail(g,64);return -1;
 }
 return 1;
}
static void packet(unsigned char*p){for(ns_u32 i=0;i<708;i++)p[i]=0;}
static int scan_peer(struct g1*g){
 unsigned char p[708];struct ns_remote remote={0,0};
 void*t=nsp_alloc(616);if(!t){fail(g,65);return 0;}g->scan_token=(ns_u32)t;
 for(ns_u32 i=0;i<154;i++)WORD((ns_u32)t+i*4)=0;WORD((ns_u32)t+12)=(ns_u32)g->scan_cb;
 ns_ble_scan(NS_BAND11,p,708,(ns_u32)t);
 /* Scan RPC has its own result layout: firmware status at12, remote at12/16
  * for the accepted scan command. Existing ABI1 checks only transport here. */
 if(!live(g))return 0;ns_u32 rc=managed_rpc(g,&g->client,p,0xed);ns_ble_remote(NS_BAND11,p,708,&remote);
 if(rc||!(remote.lo|remote.hi)){g->rpc_status=rc;fail(g,66);return 0;}
 int ok=wait_for(g,&g->scan_started,1,500,0)&&wait_for(g,&g->peer_found,1,1000,0);
 if(live(g)){
  ns_ble_stop(NS_BAND11,p,708,remote);if(managed_rpc(g,&g->client,p,0xee))fail(g,67);
  if(!wait_for(g,&g->scan_stopped,1,500,1))ok=0;
 }
 return ok&&!ld(&g->error)&&!ld(&g->stop);
}
static void packet_handle(struct g1*g,unsigned char*p){packet(p);WORD((ns_u32)p+60)=WORD(g->handle+8);}
static int disconnect(struct g1*g){
 if(!g->handle||!live(g))return 0;struct ns3_binding*b=&g->bindings[(g->round-1)&1];st(&g->phase,8);
 if(b->ever_attempted&&!b->terminal){
  unsigned char p[708];packet_handle(g,p);if(operation(g,p,0x77)){fail(g,68);return 0;}
  if(!wait_for(g,&b->terminal,1,500,1))return 0;
 }
 if(nsg3_retire_remote(&g->ui,&g->client,b,&g->retire_guard,g->sync)!=NS_OK){fail(g,69);return 0;}
 g->created_tokens++;inc(&g->barrier);st(&g->retire_done,0);st(&g->retire_ready,1);nsg3_wake_ui(g);
 if(!wait_for(g,&g->retire_done,1,500,1)||g->retire_result){fail(g,93);return 0;}
 return 1;
}
static void *worker(void*arg){
 struct g1*g=arg;unsigned char p[708];
 if(!nsp_worker_identity(&g->ui,&g->actual_tid)||!nsr_borrow(&g->ui,&g->client)||!g->client.external){fail(g,70);goto done;}
 if(ld(&g->stop))goto done;
 st(&g->phase,1);if(!scan_peer(g))goto done;
 if(ld(&g->stop)||!live(g))goto done;st(&g->phase,2);
 ns_u32 rc=0;
 for(ns_u32 round=1;round<=65535;round++){
  if(ld(&g->stop)||ld(&g->error)||!live(g))break;
  st(&g->disconnect_request,0);
  while(ld(&g->connect_request)<round&&!ld(&g->stop)&&!ld(&g->error))nsg3_wait_worker(g);
  if(ld(&g->stop)||ld(&g->error))break;
  if(round>1&&!wait_for(g,&g->ticks,ld(&g->ticks)+5,50,0))break;
  st(&g->round,round);st(&g->disconnect_request,0);st(&g->payload_limit,20);
  ns_u32 slot=(round-1)&1;struct ns3_binding*b=&g->bindings[slot];g->slot_generation[slot]=round;
  b->rpc_context=g;b->rpc_call=managed_rpc;b->fence_call=managed_fence;
  if(nsg3_create(&g->ui,&g->client,b,&g->gatt_cb[slot*13])!=NS_OK){fail(g,71);break;}
  g->created_tokens++;g->tokens[slot]=g->handle=b->token;st(&g->bind_done,0);st(&g->bind_ready,1);nsg3_wake_ui(g);
  if(!wait_for(g,&g->bind_done,1,500,1)||b->state!=N3_LINKED){fail(g,92);break;}
  g->service_start=g->service_end=g->characteristic=0;st(&g->phase,3);b->ever_attempted=1;
  packet_handle(g,p);for(ns_u32 i=0;i<6;i++)p[68+i]=g->peer[i];p[74]=g->peer[6];rc=operation(g,p,0x76);
  if(rc){fail(g,72);break;}
  if(!wait_for(g,&g->connected,round,1000,0))break;
  if(ld(&g->stop)||!live(g))break;
  st(&g->phase,4);packet_handle(g,p);rc=operation(g,p,0x78);
  if(rc||!wait_for(g,&g->discovered,round,1000,0)){if(rc||(!ld(&g->stop)&&!ld(&g->disconnect_request)))fail(g,73);break;}
  if(!g->service_start){fail(g,74);break;}
  for(ns_u32 h=g->service_start;h<=g->service_end;h++){
   if(ld(&g->stop)||ld(&g->error))break;packet(p);WORD((ns_u32)p+60)=WORD(g->handle+8);p[68]=(unsigned char)h;p[69]=(unsigned char)(h>>8);
   int found=attribute(g,p,h);if(found<0)break;if(!found)continue;
   /* Result attr_desc at20: type0, handle2, uuid4, props24. */
   if(p[24]==16&&uuid(p+28,g->config.channel_uuid)&&p[20]==3){g->characteristic=(ns_u32)p[22]|((ns_u32)p[23]<<8);if((WORD((ns_u32)p+44)&0x1a)!=0x1a)fail(g,75);break;}
  }
  if(!g->characteristic){fail(g,76);break;}
  if(ld(&g->stop)||!live(g))break;
  st(&g->phase,5);packet_handle(g,p);p[68]=g->characteristic;p[69]=g->characteristic>>8;p[70]=1;rc=operation(g,p,0x7e);
  if(rc||!wait_for(g,&g->subscribed,round,500,0)){if(rc||(!ld(&g->stop)&&!ld(&g->disconnect_request)))fail(g,77);break;}
  if(ld(&g->stop)||!live(g))break;
  st(&g->state,NS3_READY);nsg3_wake_ui(g);
  st(&g->phase,6);packet_handle(g,p);p[68]=g->characteristic;p[69]=g->characteristic>>8;rc=operation(g,p,0x7b);
  if(rc||!wait_for(g,&g->read,round,500,0)){if(rc||(!ld(&g->stop)&&!ld(&g->disconnect_request)))fail(g,78);break;}
  while(!ld(&g->stop)&&!ld(&g->error)&&!ld(&g->disconnect_request)){
   if(ld(&g->tx_write)>ld(&g->tx_read)&&!ld(&g->tx_event_ready)){
    ns_u32 at=g->tx_read&3;st(&g->phase,7);ns_u32 before=ld(&g->written);packet_handle(g,p);
    p[68]=g->characteristic;p[69]=g->characteristic>>8;p[70]=g->tx_queue[at].length;
    for(ns_u32 i=0;i<g->tx_queue[at].length;i++)p[72+i]=g->tx_queue[at].bytes[i];
    rc=operation(g,p,0x7c);
    if(rc||!wait_for(g,&g->written,before+1,500,0)){if(rc||(!ld(&g->stop)&&!ld(&g->disconnect_request)))fail(g,79);break;}
    g->tx_event_id=g->tx_queue[at].id;st(&g->tx_event_ready,1);nsg3_wake_ui(g);
    while(ld(&g->tx_event_ready)&&!ld(&g->stop)&&!ld(&g->disconnect_request))nsg3_wait_worker(g);
   }else nsg3_wait_worker(g);
  }
  if(ld(&g->stop)||ld(&g->error))break;
  if(!disconnect(g))break;inc(&g->complete);st(&g->state,NS3_DISCONNECTED);nsg3_wake_ui(g);
 }
 if(g->handle&&g->bindings[(g->round-1)&1].state==N3_LINKED)disconnect(g);

done:st(&g->done,1);nsg3_wake_ui(g);return (void*)G1_RETURN;
}

static int valid(struct g1*g){return nsp_valid(g,sizeof(*g))&&g->tag==G1_TAG;}
static int api(struct g1*g){return valid(g)&&nsp_ui_matches(&g->ui);}
enum ns3_result ns3_open(const struct ns3_config*c,ns3_link**out){
 if(!c||!out||c->size!=sizeof(*c)||c->abi!=3||!c->observer||!c->runtime)return NS3_INVALID;
 if(*out)return NS3_BUSY;struct ns3_runtime*r=c->runtime;
 if(!nsp_valid(r,sizeof(*r))||r->tag!=NS3_RUNTIME_TAG||!r->ready)return NS3_INVALID;
 if(!nsp_ui_matches(&r->ui))return NS3_CONTEXT;
 if(!nsr_live(&r->ui,&r->client))return NS3_FAILED;
 struct g1*g=nsp_alloc(sizeof(*g));if(!g||!nsp_valid(g,sizeof(*g)))return NS3_FAILED;
 for(ns_u32 i=0;i<sizeof(*g)/4;i++)((ns_u32*)g)[i]=0;g->tag=G1_TAG;g->ui=r->ui;g->client=r->client;g->config=*c;g->payload_limit=20;g->runtime=r;
 if(!g->config.operation_timeout_ms)g->config.operation_timeout_ms=5000;
 if(!g->config.connect_timeout_ms)g->config.connect_timeout_ms=10000;
 if(g->config.operation_timeout_ms>60000||g->config.connect_timeout_ms>60000){nsp_free(g);return NS3_INVALID;}
 if(FN(0xc8bbb10,int(*)(void*,int,unsigned int))(g->wake_sem,0,0)){nsp_free(g);return NS3_FAILED;}g->sem_ready=1;
 g->scan_cb[0]=16;g->scan_cb[1]=(ns_u32)scan_result;g->scan_cb[2]=(ns_u32)scan_start;g->scan_cb[3]=(ns_u32)scan_stop;
 for(ns_u32 i=0;i<2;i++){
  ns_u32*cb=&g->gatt_cb[i*13];cb[0]=52;cb[1]=(ns_u32)on_connect;cb[2]=(ns_u32)on_disconnect;cb[3]=(ns_u32)on_discover;
  cb[4]=(ns_u32)on_read;cb[5]=(ns_u32)on_write;cb[6]=(ns_u32)on_sub;cb[7]=(ns_u32)on_notify;cb[8]=(ns_u32)on_mtu;
 }
 g->next=r->head;r->head=g;r->opened++;r->live++;r->bytes+=sizeof(*g);*out=g;return NS3_OK;
}
enum ns3_result ns3_connect(ns3_link*g){
 if(!api(g))return NS3_CONTEXT;if(ld(&g->stop)||ld(&g->error))return NS3_FAILED;
 if(g->connect_request>=65535)return NS3_LIMIT;
 if(ld(&g->state)!=NS3_IDLE&&ld(&g->state)!=NS3_DISCONNECTED)return NS3_BUSY;
 st(&g->state,NS3_CONNECTING);inc(&g->connect_request);nsg3_enable_timer(g);
 if(!g->tid&&(nsp_spawn(&g->tid,worker,g)||!g->tid||g->tid==g->ui.tid)){fail(g,81);st(&g->stop,1);return NS3_HELD;}
 nsg3_signal_worker(g);return NS3_OK;
}
enum ns3_result ns3_send(ns3_link*g,const void*p,ns_u32 n,ns_u32 id){
 if(!api(g))return NS3_CONTEXT;if(!p||!n||n>NS3_TX_LIMIT)return NS3_SIZE;
 if(ld(&g->state)!=NS3_READY||ld(&g->disconnect_request)||ld(&g->stop))return NS3_BUSY;
 ns_u32 w=ld(&g->tx_write);if(w-ld(&g->tx_read)>=4)return NS3_BUSY;
 const unsigned char*b=p;ns_u32 at=w&3;for(ns_u32 i=0;i<n;i++)g->tx_queue[at].bytes[i]=b[i];g->tx_queue[at].length=n;g->tx_queue[at].id=id;
 st(&g->tx_write,w+1);nsg3_signal_worker(g);nsg3_enable_timer(g);return NS3_OK;
}
enum ns3_result ns3_disconnect(ns3_link*g){
 if(!api(g))return NS3_CONTEXT;
 if(ld(&g->state)==NS3_DISCONNECTED||ld(&g->state)==NS3_DISCONNECTING)return NS3_OK;
 if(ld(&g->state)!=NS3_READY&&ld(&g->state)!=NS3_CONNECTING)return NS3_BUSY;
 st(&g->disconnect_request,1);st(&g->state,NS3_DISCONNECTING);nsg3_signal_worker(g);nsg3_enable_timer(g);return NS3_OK;
}
enum ns3_result ns3_close(ns3_link*g){
 if(!api(g))return NS3_CONTEXT;if(ld(&g->state)==NS3_CLOSED_HELD||ld(&g->state)==NS3_TRANSPORT_HELD)return NS3_HELD;
 st(&g->close_request,1);st(&g->stop,1);st(&g->state,NS3_CLOSING);nsg3_signal_worker(g);nsg3_enable_timer(g);nsg3_wake_ui(g);return NS3_OK;
}
static void emit(struct g1*g,ns_u32 type,const unsigned char*bytes,ns_u32 length){
 if(!g->config.observer)return;
 struct ns3_event e={type,ld(&g->round),g->tx_request_id,g->error,length,bytes};
 g->dispatching=1;g->config.observer(g,&e,g->config.user);g->dispatching=0;
}
static void report(struct g1*g){
 ns_u32*r=g->report;
 r[0]=g->phase;r[1]=ld(&g->error);r[2]=g->round;r[3]=g->complete;r[4]=g->scan_started;r[5]=g->scan_stopped;
 r[6]=g->connected;r[7]=g->disconnected;r[8]=g->discovered;r[9]=g->subscribed;r[10]=g->read;r[11]=g->written;r[12]=g->notified;
 r[13]=g->exit_proof;r[14]=g->joined;r[15]=g->barrier;r[16]=g->rpc_status;r[17]=g->gatt_status;r[18]=g->callback_tid;
 r[19]=ld(&g->active);r[20]=g->late;r[21]=g->payload_limit;r[22]=g->stop;r[23]=g->done;r[24]=g->handle!=0;r[25]=g->unexpected_disconnect;

}
enum ns3_result ns3_snapshot_get(ns3_link*g,struct ns3_snapshot*s){
 if(!api(g))return NS3_CONTEXT;if(!s||s->size!=sizeof(*s))return NS3_INVALID;
 report(g);s->abi=3;s->state=ld(&g->state);s->generation=ld(&g->round);s->error=g->error;s->tx_limit=20;s->payload_limit=g->payload_limit;s->retained_tokens=(g->tokens[0]!=0)+(g->tokens[1]!=0);
 for(ns_u32 i=0;i<26;i++)s->report[i]=g->report[i];s->report[24]=s->retained_tokens;
 ns_u32*d=s->diagnostics;
 d[0]=g->mtu_events;d[1]=g->mtu_warnings;d[2]=g->mtu_zero;d[3]=g->mtu_invalid;
 d[4]=g->mtu_first_bad_status;d[5]=g->mtu_first_bad_value;d[6]=g->mtu_first_bad_round;d[7]=g->mtu_first_bad_phase;
 d[8]=g->mtu_last_status;d[9]=g->mtu_last_value;d[10]=g->payload_limit;
 d[11]=g->attr_queries;d[12]=g->attr_misses;d[13]=g->attr_first_miss;d[14]=g->attr_first_transport;d[15]=g->attr_first_status;d[16]=g->attr_first_handle;d[17]=g->attr_first_round;d[18]=g->attr_first_phase;
 d[19]=g->read_last_status|(g->read_bad_reason<<8);d[20]=g->read_last_handle|(g->read_last_length<<16);d[21]=g->read_cancelled;
 s->created_tokens=g->created_tokens;s->collected_tokens=g->collected_tokens;s->queued_tx=g->tx_write-g->tx_read;s->queued_rx=g->rx_write-g->rx_read;s->rpc_active=g->rpc_active;s->rpc_command=g->rpc_command;
 return NS3_OK;
}
void nsg3_pump(struct g1*g){
 if(!valid(g)||!nsp_ui_matches(&g->ui))return;
 if(ld(&g->bind_ready)&&!ld(&g->active)){
  struct ns3_binding*b=&g->bindings[(g->round-1)&1];enum ns_result r=nsg3_bind_ui(&g->ui,&g->client,b);
  if(r!=NS_OK)fail(g,92);st(&g->bind_ready,0);st(&g->bind_done,1);nsg3_signal_worker(g);
 }
 if(ld(&g->retire_ready)&&!ld(&g->active)){
  enum ns_result r=nsg3_collect_ui(&g->ui,&g->client,&g->bindings[(g->round-1)&1],&g->retire_guard,g->scan_token);
  if(r!=NS_BUSY){g->retire_result=r;st(&g->retire_ready,0);if(r==NS_OK){g->tokens[(g->round-1)&1]=0;g->handle=0;g->collected_tokens+=2;}st(&g->retire_done,1);nsg3_signal_worker(g);}
 }
 if(ld(&g->rpc_active)&&g->ticks>=g->rpc_deadline&&!g->seen_transport_held){
  fail(g,90);st(&g->stop,1);st(&g->state,NS3_TRANSPORT_HELD);g->transport_held=g->seen_transport_held=1;
  emit(g,NS3_EVENT_TRANSPORT_HELD,0,0);g->config.observer=0;g->config.user=0;nsg3_signal_worker(g);
 }
 if(ld(&g->error)){st(&g->stop,1);nsg3_signal_worker(g);if(!g->seen_error){g->seen_error=1;emit(g,NS3_EVENT_ERROR,0,0);}}
 if(ld(&g->state)==NS3_READY&&g->seen_ready<ld(&g->round)){g->seen_ready=g->round;emit(g,NS3_EVENT_READY,0,0);}
 if(ld(&g->read_pending)){emit(g,NS3_EVENT_INITIAL_READ,g->read_bytes,g->read_size);st(&g->read_pending,0);}
 if(ld(&g->tx_event_ready)){g->tx_request_id=g->tx_event_id;inc(&g->tx_read);st(&g->tx_event_ready,0);g->seen_tx++;emit(g,NS3_EVENT_WRITE_COMPLETE,0,0);nsg3_signal_worker(g);}
 for(ns_u32 count=0;count<8&&g->rx_read<g->rx_write;count++){
  ns_u32 at=g->rx_read&7;
  if(g->rx_queue[at].generation==g->round)emit(g,NS3_EVENT_RECEIVED,g->rx_queue[at].bytes,g->rx_queue[at].length);else inc(&g->late);
  inc(&g->rx_read);
 }
 if(ld(&g->state)==NS3_DISCONNECTED&&g->seen_disconnect<ld(&g->complete)){g->seen_disconnect=g->complete;emit(g,NS3_EVENT_DISCONNECTED,0,0);}
 if(ld(&g->done)&&g->tid&&!g->joined){
  enum ns_result r=nsp_exit_proof(&g->ui,g->tid,G1_RETURN);
  if(r==NS_INVALID)fail(g,83);
  if(r==NS_OK){void*v=0;g->exit_proof=1;if(nsp_join(g->tid,&v)||(ns_u32)v!=G1_RETURN||g->tid!=g->actual_tid)fail(g,84);else g->joined=1;}
 }
 if((g->joined||(!g->tid&&ld(&g->stop)))&&!g->seen_closed){
  while(g->tx_read<g->tx_write){g->tx_request_id=g->tx_queue[g->tx_read&3].id;inc(&g->tx_read);emit(g,NS3_EVENT_WRITE_CANCELED,0,0);}
  g->seen_closed=1;st(&g->state,NS3_CLOSED_HELD);emit(g,NS3_EVENT_CLOSED_HELD,0,0);g->config.observer=0;g->config.user=0;
 }
 report(g);
 int need=(!g->seen_closed&&(g->state==NS3_CONNECTING||g->state==NS3_DISCONNECTING||g->state==NS3_CLOSING||g->wait_deadline||g->rpc_active||g->retire_ready||g->bind_ready||g->done));
 if(g->transport_held&&g->rpc_active)need=0;
 if(need)nsg3_enable_timer(g);else if(g->timer){nsp_timer_delete(g->timer);g->timer=0;}
}
void nsg3_tick(void*t){
 struct g1*g=nsp_timer_data(t);if(!valid(g))return;inc(&g->ticks);
 if(g->wait_deadline&&g->ticks>=g->wait_deadline)nsg3_signal_worker(g);
 nsg3_pump(g);
}
enum ns3_result ns3_destroy(ns3_link**slot){
 if(!slot||!*slot)return NS3_INVALID;struct g1*g=*slot;if(!api(g))return NS3_CONTEXT;
 if(g->dispatching||g->runtime->pumping||!g->seen_closed||g->timer||g->active||g->rpc_active||(g->tid&&!g->joined))return NS3_HELD;
 for(ns_u32 i=0;i<2;i++)if(g->bindings[i].token||g->bindings[i].node)return NS3_HELD;
 if(g->retire_guard.token||g->retire_guard.node||g->bind_ready||g->retire_ready)return NS3_HELD;
 if(g->scan_token&&g->scan_started!=g->scan_stopped)return NS3_HELD;
 if(nsg3_queue_clear(&g->ui,&g->client,0,0,g->scan_token)!=NS_OK||nsr_timers_clear(g,sizeof(*g),g->scan_cb,0,g->scan_token,0)!=NS_OK)return NS3_HELD;
 struct ns3_runtime*r=g->runtime;struct g1**p=&r->head;while(*p&&*p!=g)p=&(*p)->next;if(*p!=g)return NS3_HELD;*p=g->next;
 FN(0xc3f5104,void(*)(void*))(g->wake_sem);g->sem_ready=0;r->destroyed++;r->live--;r->bytes-=sizeof(*g);g->tag=0;*slot=0;nsp_free(g);return NS3_OK;
}
