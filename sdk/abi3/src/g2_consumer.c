#include "g2_fixture.h"
#include "ns_port.h"
static void check(struct g2_fixture*f,int ok,ns3_u32 error){if(!ok&&!f->app_error)f->app_error=error;}
static ns3_u32 payload(unsigned char*p,ns3_u32 round,ns3_u32 index){
 ns3_u32 n=index==0?1:index==2?20:8;for(ns3_u32 i=0;i<n;i++)p[i]=(unsigned char)(0x40+round+index*23+i);return n;
}
static void observe(ns3_link*l,const struct ns3_event*e,void*user){
 struct g2_fixture*f=user;
 if(e->type==NS3_EVENT_READY){
  f->ready_events++;f->tx_mask=f->rx_mask=f->round_reads=0;
  if(f->test_case==2){f->closing=1;check(f,ns3_close(l)==NS3_OK,101);}
  else if(f->test_case==3){const unsigned char fail[8]={'G','4','-','F','A','I','L','!'};check(f,ns3_send(l,fail,8,900)==NS3_OK,102);}
  else {
   unsigned char p[20];for(ns3_u32 i=0;i<4;i++){ns3_u32 n=payload(p,e->generation,i);check(f,ns3_send(l,p,n,e->generation*10+i)==NS3_OK,103);for(ns3_u32 j=0;j<20;j++)p[j]=0xa5;}
   check(f,ns3_send(l,p,1,999)==NS3_BUSY,104);f->busy++;
  }
 }else if(e->type==NS3_EVENT_INITIAL_READ){
  const unsigned char v[8]={'G','1','-','R','E','A','D','!'};int ok=e->length==8;for(ns3_u32 i=0;ok&&i<8;i++)if(e->bytes[i]!=v[i])ok=0;check(f,ok,105);f->read_events++;f->round_reads=1;
 }else if(e->type==NS3_EVENT_RECEIVED){
  unsigned char p[20];ns3_u32 bit=0;for(ns3_u32 i=0;i<4;i++){ns3_u32 n=payload(p,e->generation,i);int ok=n==e->length;for(ns3_u32 j=0;ok&&j<n;j++)if(p[j]!=e->bytes[j])ok=0;if(ok)bit=1u<<i;}
  check(f,bit&&!(f->rx_mask&bit),106);f->rx_mask|=bit;f->rx_events++;
 }else if(e->type==NS3_EVENT_WRITE_COMPLETE){
  ns3_u32 i=e->request_id-e->generation*10;check(f,i<4,107);if(i<4){check(f,!(f->tx_mask&(1u<<i)),108);f->tx_mask|=1u<<i;}f->tx_events++;
 }else if(e->type==NS3_EVENT_DISCONNECTED){
  f->disconnect_events++;check(f,(e->generation<8?ns3_connect(l):ns3_close(l))==NS3_OK,109);if(e->generation==8)f->closing=1;
 }else if(e->type==NS3_EVENT_ERROR){
  if(f->test_case==3&&e->error==54)f->expected_error++;else check(f,0,e->error?e->error:110);ns3_close(l);f->closing=1;
 }else if(e->type==NS3_EVENT_TRANSPORT_HELD){check(f,0,111);f->done=1;
 }else if(e->type==NS3_EVENT_CLOSED_HELD){f->case_closed=1;f->closed_events++;}
 if(f->test_case==4&&f->tx_mask==15&&f->rx_mask==15&&f->round_reads&&!f->closing&&e->type!=NS3_EVENT_DISCONNECTED){check(f,ns3_disconnect(l)==NS3_OK,112);f->tx_mask=f->rx_mask=f->round_reads=0;}
 if(f->app_error&&!f->case_closed)ns3_close(l);
}
static void start_case(struct g2_fixture*f){
 struct ns3_config c={0};c.size=sizeof(c);c.abi=NS3_ABI_CANDIDATE;c.observer=observe;c.user=f;c.runtime=f->runtime;
 const unsigned char u[16]={0x91,0xd1,0,1,0xd0,0xb2,0x4c,0xf6,0x91,0x36,0xf1,0x6d,0xb5,0x5b,0x6e,0x11};
 for(ns3_u32 i=0;i<16;i++)c.service_uuid[i]=c.channel_uuid[i]=u[i];c.channel_uuid[3]=2;
 f->test_case++;f->closing=f->case_closed=f->destroy_tries=0;check(f,ns3_open(&c,&f->link)==NS3_OK,113);
 if(f->link){check(f,ns3_connect(f->link)==NS3_OK,114);if(f->test_case==1){check(f,ns3_close(f->link)==NS3_OK,115);f->closing=1;}}else f->done=1;
}
__attribute__((used,section(".text.entry"))) void owned_callback(void*t){
 struct g2_fixture*f=nsp_timer_data(t);if(!nsp_valid(f,sizeof(*f))||f->tag!=G2_TAG)return;f->ticks++;
 if(!f->started){f->started=1;check(f,ns3_runtime_open(&f->runtime)==NS3_OK,116);if(!f->app_error)start_case(f);}
 ns3_u32 h=__atomic_load_n(&f->heartbeat,__ATOMIC_ACQUIRE);f->stale=h==f->last_heartbeat?f->stale+1:0;f->last_heartbeat=h;
 if(f->started&&f->stale>=100)check(f,0,123);
 if(f->link){
  if(f->cancel||f->ticks>=1800||f->app_error)ns3_close(f->link);
  f->snapshot.size=sizeof(f->snapshot);check(f,ns3_snapshot_get(f->link,&f->snapshot)==NS3_OK,117);
  for(ns3_u32 i=0;i<22;i++)f->diagnostics[i]=f->snapshot.diagnostics[i];
  f->diagnostics[21]+=f->read_cancels;
  if(f->case_closed){
   struct ns3_snapshot*s=&f->snapshot;
   check(f,s->retained_tokens==0&&s->created_tokens==s->collected_tokens,118);

   if(ns3_destroy(&f->link)==NS3_OK){f->read_cancels+=s->diagnostics[21];f->created+=s->created_tokens;f->collected+=s->collected_tokens;f->joins+=s->report[14];f->exits+=s->report[13];f->barriers+=s->report[15];f->connections+=s->report[6];if(!f->app_error)f->passed++;f->case_closed=0;if(f->test_case<4&&!f->app_error)start_case(f);}
   else if(++f->destroy_tries>=50)check(f,0,119);
  }
 }
 if(f->runtime){f->rs.size=sizeof(f->rs);check(f,ns3_runtime_snapshot_get(f->runtime,&f->rs)==NS3_OK,120);}
 if(f->passed==4&&!f->link){f->quiet++;if(f->quiet>=20){check(f,f->rs.sessions_live==0&&f->rs.session_bytes_live==0&&f->rs.sessions_opened==4&&f->rs.sessions_destroyed==4&&f->expected_error==1&&f->busy==8&&f->rx_events==32&&f->tx_events==32,121);f->done=1;}}
 if(f->ticks>=1800){check(f,0,122);f->done=1;}
 if(f->app_error&&(!f->link||(f->case_closed&&f->destroy_tries>=50)))f->done=1;
 ns3_u32*r=f->report;r[0]=f->test_case;r[1]=f->app_error;r[2]=f->test_case;r[3]=f->passed;r[4]=f->rs.sessions_opened;r[5]=f->rs.sessions_destroyed;r[6]=f->rs.sessions_live;r[7]=f->rs.session_bytes_live;r[8]=f->connections;r[9]=f->disconnect_events;r[10]=f->read_events;r[11]=f->tx_events;r[12]=f->rx_events;r[13]=f->created;r[14]=f->collected;r[15]=f->joins;r[16]=f->exits;r[17]=f->barriers;r[18]=f->expected_error;r[19]=f->busy;r[20]=f->snapshot.report[18];r[21]=f->snapshot.report[19];r[22]=f->closed_events;r[23]=f->done;r[24]=f->quiet;r[25]=f->timer!=0;
 if(f->done){nsp_timer_delete(t);f->timer=0;r[25]=0;}
}
