#include "ns_link_v3.h"
struct fixed_demo {ns3_runtime*runtime;ns3_link*link;unsigned int received,closed,error;};
static void changed(ns3_link*l,const struct ns3_event*e,void*user){
 struct fixed_demo*d=user;
 if(e->type==NS3_EVENT_READY){const unsigned char bytes[1]={0x41};if(ns3_send(l,bytes,1,1)!=NS3_OK)d->error=1;}
 else if(e->type==NS3_EVENT_RECEIVED){/* Copy e->bytes before returning if needed. */d->received++;ns3_disconnect(l);}
 else if(e->type==NS3_EVENT_DISCONNECTED){/* At most three explicit attempts, minimum SDK backoff500ms. */if(e->generation<3)ns3_connect(l);else ns3_close(l);}
 else if(e->type==NS3_EVENT_ERROR){d->error=e->error;ns3_close(l);}
 else if(e->type==NS3_EVENT_CLOSED_HELD)d->closed=1;
 else if(e->type==NS3_EVENT_TRANSPORT_HELD){d->error=e->error;/* Keep SDK resources until real completion; no forced free. */}
}
enum ns3_result fixed_demo_start(struct fixed_demo*d){
 if(!d->runtime){enum ns3_result r=ns3_runtime_open(&d->runtime);if(r!=NS3_OK)return r;}
 struct ns3_config c={0};c.size=sizeof(c);c.abi=NS3_ABI_CANDIDATE;c.runtime=d->runtime;c.observer=changed;c.user=d;
 const unsigned char u[16]={0x91,0xd1,0,1,0xd0,0xb2,0x4c,0xf6,0x91,0x36,0xf1,0x6d,0xb5,0x5b,0x6e,0x11};
 for(unsigned int i=0;i<16;i++)c.service_uuid[i]=c.channel_uuid[i]=u[i];c.channel_uuid[3]=2;
 enum ns3_result r=ns3_open(&c,&d->link);return r==NS3_OK?ns3_connect(d->link):r;
}
enum ns3_result fixed_demo_collect(struct fixed_demo*d){return d->closed?ns3_destroy(&d->link):NS3_BUSY;}
/* runtime + module remain resident. Call collect outside any SDK observer.
 * Add an application deadline if waiting for an application response.
 * WRITE_COMPLETE is an ATT response, never a delivery acknowledgement.
 * No application calls firmware addresses, RPC, tokens or thread functions. */
