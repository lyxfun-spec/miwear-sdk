#include "ns_sdk_page.h"
static ns_u32 sdk_ui_valid(void *arg){struct ns_ble_backend *b=arg;return nsp_valid(b,sizeof(*b))&&nsp_ui_matches(&b->ui);}
static void sdk_heartbeat(void *arg){nsb_heartbeat(arg);}
enum ns_result ns5page_begin(struct ns_sdk_page *p,const struct ns_ui_context *ui,const struct nss_config *cfg){
 if(!nsp_valid(p,sizeof(*p))||p->flow.initialized||!ui||!cfg||cfg->abi!=NS_SDK_ABI||!cfg->roles||(cfg->roles&~3u)||cfg->retain_after_join_polls>1000u||!nsp_guards()||!nsr_guards())return NS_INVALID;
 enum ns_result r=nsb_init(&p->ble,ui);if(r!=NS_OK)return r;
 p->ble.limit=0;struct nss_hooks h={sdk_ui_valid,sdk_heartbeat,&p->ble};
 return nss_begin(&p->flow,&p->ble.controller,cfg,&h);
}
enum ns_result ns5page_resume(struct ns_sdk_page *p){return nsp_valid(p,sizeof(*p))?nss_show(&p->flow):NS_INVALID;}
enum ns_result ns5page_pause(struct ns_sdk_page *p){return nsp_valid(p,sizeof(*p))?nss_hide(&p->flow):NS_INVALID;}
enum ns_result ns5page_stop(struct ns_sdk_page *p,ns_u32 reason){return nsp_valid(p,sizeof(*p))?nss_stop(&p->flow,reason):NS_INVALID;}
enum ns_result ns5page_poll(struct ns_sdk_page *p){return nsp_valid(p,sizeof(*p))?nss_poll(&p->flow):NS_INVALID;}
enum ns_result ns5page_observe(const struct ns_sdk_page *p,struct nss_snapshot *s){return nsp_valid(p,sizeof(*p))?nss_observe(&p->flow,s):NS_INVALID;}
