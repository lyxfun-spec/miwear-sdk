#include "foundation_component.h"
enum ns_result foundation_begin(struct foundation_component *c,const struct ns_ui_context *ui){
 if(!c)return NS_INVALID;
 const struct nss_config cfg={NS_SDK_ABI,NS_SCAN|NS_ADV,0u};
 return ns5page_begin(&c->session,ui,&cfg);
}
enum ns_result foundation_show(struct foundation_component *c){return c?ns5page_resume(&c->session):NS_INVALID;}
enum ns_result foundation_hide(struct foundation_component *c){return c?ns5page_pause(&c->session):NS_INVALID;}
enum ns_result foundation_stop(struct foundation_component *c){return c?ns5page_stop(&c->session,NS_USER):NS_INVALID;}
enum ns_result foundation_tick(struct foundation_component *c){return c?ns5page_poll(&c->session):NS_INVALID;}
enum ns_result foundation_observe(const struct foundation_component *c,struct nss_snapshot *s){return c?ns5page_observe(&c->session,s):NS_INVALID;}
