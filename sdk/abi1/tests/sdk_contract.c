#include "ns_sdk_flow.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,cases,failed;
#define CK(x) do{checks++;if(!(x)){failed=__LINE__;return;}}while(0)
struct model {unsigned ui,ready,done,exited,quiet,fault,join_fail,starts,joins,frees,stops,inspects,beats,roles,owner;struct ns_manager *manager;struct ns_handle handle;};
struct fixture {struct nss_flow flow;struct ns_controller c;struct model m;};
static ns_u32 ui(void *x){return ((struct model*)x)->ui;}
static void beat(void *x){((struct model*)x)->beats++;}
static enum ns_result start(void *x,struct ns_manager *m,struct ns_handle h,ns_u32 roles){
 struct model *p=x;if(p->owner)return NS_INVALID;p->starts++;p->owner=1;p->roles=roles;p->manager=m;p->handle=h;p->ready=p->done=p->exited=p->quiet=0;return NS_OK;
}
static void stop(void*x,struct ns_handle h){struct model*p=x;if(h.generation==p->handle.generation)p->stops++;}
static enum ns_result inspect(void*x,struct ns_handle h,struct ns_evidence*e){
 struct model*p=x;p->inspects++;if(p->fault)return NS_INVALID;
 if(p->ready)(void)ns_worker_ready(p->manager,h);if(p->done)(void)ns_worker_done(p->manager,h);
 e->generation=h.generation;e->terminal_roles=p->done?p->roles:0;e->internal_barrier=p->done;e->worker_exited=p->exited;e->external_ui=1;e->own_queue_clear=p->quiet;e->own_timers_clear=p->quiet;return NS_OK;
}
static enum ns_result join(void*x,struct ns_handle h){struct model*p=x;(void)h;if(!p->exited||p->join_fail)return NS_INVALID;p->joins++;return NS_OK;}
static enum ns_result release(void*x,struct ns_handle h){struct model*p=x;(void)h;if(!p->exited||!p->quiet||p->joins!=p->frees+1)return NS_INVALID;p->frees++;p->owner=0;return NS_OK;}
static const struct ns_backend ops={start,stop,inspect,join,release};
static void init(struct fixture*f,unsigned retain,unsigned roles){
 memset(f,0,sizeof(*f));f->m.ui=1;CK(ns_controller_init(&f->c,&ops,&f->m)==NS_OK);
 struct nss_config c={NS_SDK_ABI,roles,retain};struct nss_hooks h={ui,beat,&f->m};CK(nss_begin(&f->flow,&f->c,&c,&h)==NS_OK);
}
static void ready(struct fixture*f){CK(nss_show(&f->flow)==NS_OK);CK(nss_poll(&f->flow)==NS_OK);f->m.ready=1;CK(nss_poll(&f->flow)==NS_OK);CK(ns_state(&f->c.manager)==NS_ACTIVE);}
static void finish(struct fixture*f){f->m.done=f->m.exited=f->m.quiet=1;for(unsigned i=0;i<40;i++)(void)nss_poll(&f->flow);}
static void test_begin(void){
 struct fixture f={0};f.m.ui=1;CK(ns_controller_init(&f.c,&ops,&f.m)==NS_OK);struct nss_config c={NS_SDK_ABI,NS_SCAN,0};struct nss_hooks h={ui,beat,&f.m};
 c.abi=0;CK(nss_begin(&f.flow,&f.c,&c,&h)==NS_INVALID);c.abi=NS_SDK_ABI;c.roles=0;CK(nss_begin(&f.flow,&f.c,&c,&h)==NS_INVALID);c.roles=4;CK(nss_begin(&f.flow,&f.c,&c,&h)==NS_INVALID);c.roles=NS_SCAN;c.retain_after_join_polls=1001;CK(nss_begin(&f.flow,&f.c,&c,&h)==NS_INVALID);c.retain_after_join_polls=0;
 f.m.ui=0;CK(nss_begin(&f.flow,&f.c,&c,&h)==NS_INVALID);CK(!f.flow.initialized&&!f.m.starts);f.m.ui=1;CK(nss_begin(&f.flow,&f.c,&c,&h)==NS_OK);CK(nss_begin(&f.flow,&f.c,&c,&h)==NS_INVALID);CK(f.flow.config.roles==NS_SCAN&&!f.m.starts);cases++;
}
static void test_intent_and_observe(void){
 struct fixture f;init(&f,0,3);struct nss_snapshot s={0};CK(nss_show(&f.flow)==NS_OK);CK(nss_show(&f.flow)==NS_OK);CK(f.flow.wanted_epoch==1&&!f.m.starts);struct nss_flow before=f.flow;
 for(unsigned i=0;i<32;i++){CK(nss_observe(&f.flow,&s)==NS_OK);CK(s.view==NSS_STARTING&&s.page_epoch==1&&s.session_generation==0);}
 CK(!memcmp(&before,&f.flow,sizeof(before))&&!f.m.starts&&!f.m.inspects);CK(nss_hide(&f.flow)==NS_OK);CK(nss_poll(&f.flow)==NS_OK);CK(!f.m.starts);cases++;
}
static void test_context(void){
 struct fixture f;init(&f,0,3);ready(&f);struct nss_flow before=f.flow;struct ns_controller cb=f.c;struct nss_snapshot s,b;memset(&s,0xa5,sizeof s);b=s;f.m.ui=0;
 CK(nss_show(&f.flow)==NS_INVALID);CK(nss_hide(&f.flow)==NS_INVALID);CK(nss_stop(&f.flow,NS_USER)==NS_INVALID);CK(nss_poll(&f.flow)==NS_INVALID);CK(nss_observe(&f.flow,&s)==NS_INVALID);
 CK(!memcmp(&before,&f.flow,sizeof before)&&!memcmp(&cb,&f.c,sizeof cb)&&!memcmp(&s,&b,sizeof s));f.m.ui=1;CK(nss_hide(&f.flow)==NS_OK);finish(&f);CK(f.m.frees==1);cases++;
}
static void test_defaults_and_roles(void){
 for(unsigned roles=1;roles<=3;roles++){struct fixture f;init(&f,0,roles);ready(&f);CK(f.m.roles==roles);CK(nss_hide(&f.flow)==NS_OK);finish(&f);struct nss_snapshot s;CK(nss_observe(&f.flow,&s)==NS_OK);CK(f.m.starts==f.m.joins&&f.m.joins==f.m.frees&&f.m.frees==1);CK(!f.flow.hold_polls&&s.session_quiet&&s.view==NSS_QUIET);cases++;}
}
static void test_reopen_coalesce(void){
 struct fixture f;init(&f,2,3);ready(&f);CK(nss_hide(&f.flow)==NS_OK);CK(nss_show(&f.flow)==NS_OK);(void)nss_poll(&f.flow);CK(nss_hide(&f.flow)==NS_OK);CK(nss_show(&f.flow)==NS_OK);(void)nss_poll(&f.flow);CK(f.m.starts==1&&f.flow.wanted_epoch==3&&f.flow.deferred==2);
 finish(&f);CK(f.m.starts==2&&f.m.frees==1&&f.flow.started_epoch==3);f.m.ready=1;(void)nss_poll(&f.flow);CK(nss_hide(&f.flow)==NS_OK);finish(&f);CK(f.m.starts==2&&f.m.frees==2&&f.flow.hold_polls==4);cases++;
}
static void test_fresh_proof(void){
 struct fixture f;init(&f,2,3);ready(&f);CK(nss_hide(&f.flow)==NS_OK);f.m.done=f.m.exited=1;
 for(unsigned i=0;i<8&&!f.m.joins;i++)(void)nss_poll(&f.flow);CK(f.m.joins==1&&!f.m.frees);unsigned reads=f.m.inspects;
 CK(nss_poll(&f.flow)==NS_BUSY);CK(nss_poll(&f.flow)==NS_BUSY);CK(f.m.inspects==reads&&!f.m.frees);(void)nss_poll(&f.flow);CK(f.m.inspects>reads&&!f.m.frees);
 f.m.quiet=1;(void)nss_poll(&f.flow);CK(f.m.frees==1);cases++;
}
static void test_explicit_cancel(void){
 struct fixture f;init(&f,0,3);ready(&f);CK(nss_hide(&f.flow)==NS_OK);CK(nss_show(&f.flow)==NS_OK);(void)nss_poll(&f.flow);CK(nss_stop(&f.flow,NS_USER)==NS_OK);finish(&f);CK(!f.flow.visible&&f.m.starts==1&&f.m.frees==1);CK(f.flow.stop_reason==NS_USER);CK(nss_show(&f.flow)==NS_OK);(void)nss_poll(&f.flow);CK(f.m.starts==2);cases++;
}
static void test_hold(void){
 struct fixture f;init(&f,0,3);ready(&f);f.m.fault=1;CK(nss_poll(&f.flow)==NS_INVALID);struct nss_snapshot s;CK(nss_observe(&f.flow,&s)==NS_OK&&s.view==NSS_HELD&&!s.session_quiet);CK(nss_show(&f.flow)==NS_INVALID);for(unsigned i=0;i<10;i++)CK(nss_poll(&f.flow)==NS_INVALID);CK(f.m.starts==1&&!f.m.joins&&!f.m.frees);cases++;
}
static void test_exhaustion(void){
 struct fixture f;init(&f,0,3);ready(&f);CK(nss_hide(&f.flow)==NS_OK);f.flow.wanted_epoch=0xffffffffu;CK(nss_show(&f.flow)==NS_EXHAUSTED);CK(!f.flow.visible&&f.flow.wanted_epoch==0xffffffffu&&f.flow.error);CK(nss_poll(&f.flow)==NS_INVALID&&f.m.starts==1&&!f.m.frees);cases++;
}
static void test_no_natural_restart(void){
 struct fixture f;init(&f,0,3);ready(&f);finish(&f);CK(f.flow.visible&&f.m.starts==1&&f.m.frees==1);for(unsigned i=0;i<10;i++)(void)nss_poll(&f.flow);CK(f.m.starts==1);struct nss_snapshot s;CK(nss_observe(&f.flow,&s)==NS_OK&&s.view==NSS_QUIET);cases++;
}
static void test_generation_and_bad_stop(void){
 struct fixture f;init(&f,0,3);ready(&f);struct ns_handle old=f.m.handle;CK(nss_stop(&f.flow,0)==NS_INVALID&&f.flow.visible);CK(nss_hide(&f.flow)==NS_OK);finish(&f);CK(nss_show(&f.flow)==NS_OK);(void)nss_poll(&f.flow);CK(f.m.handle.generation!=old.generation);CK(ns_worker_ready(&f.c.manager,old)==NS_STALE);CK(ns_worker_done(&f.c.manager,old)==NS_STALE);CK(ns_state(&f.c.manager)==NS_STARTING);cases++;
}
int main(void){
 void(*tests[])(void)={test_begin,test_intent_and_observe,test_context,test_defaults_and_roles,test_reopen_coalesce,test_fresh_proof,test_explicit_cancel,test_hold,test_exhaustion,test_no_natural_restart,test_generation_and_bad_stop};
 for(unsigned i=0;i<sizeof tests/sizeof tests[0]&&!failed;i++)tests[i]();
 printf("{\"cases\":%u,\"checks\":%u,\"failure_line\":%u,\"hardware_tested\":false}\n",cases,checks,failed);return failed?1:0;
}
