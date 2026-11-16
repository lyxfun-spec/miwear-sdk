#include "ns_sdk_flow.h"
static ns_u32 valid(const struct nss_flow *f){return f && f->initialized && f->controller && f->hooks.ui_valid && f->hooks.ui_valid(f->hooks.context);}
static ns_u32 busy(ns_u32 state){return state!=NS_IDLE && state!=NS_RELEASED;}
enum ns_result nss_begin(struct nss_flow *f,struct ns_controller *c,const struct nss_config *cfg,const struct nss_hooks *h){
 if(!f||!c||!cfg||!h||f->initialized||cfg->abi!=NS_SDK_ABI||!cfg->roles||(cfg->roles&~3u)||cfg->retain_after_join_polls>1000u||!h->ui_valid||!h->ui_valid(h->context)||!c->backend||ns_state(&c->manager)!=NS_IDLE)return NS_INVALID;
 f->controller=c;f->config=*cfg;f->hooks=*h;f->initialized=1;return NS_OK;
}
enum ns_result nss_show(struct nss_flow *f){
 if(!valid(f)||f->error||ns_state(&f->controller->manager)==NS_HELD)return NS_INVALID;
 if(!f->visible){
  if(f->wanted_epoch==0xffffffffu){f->error=1;(void)nss_stop(f,NS_GATE_ERROR);return NS_EXHAUSTED;}
  f->wanted_epoch++;
 }
 f->visible=1;return NS_OK;
}
enum ns_result nss_stop(struct nss_flow *f,ns_u32 reason){
 if(!valid(f)||reason<NS_USER||reason>NS_GATE_ERROR)return NS_INVALID;
 ns_u32 state=ns_state(&f->controller->manager);
 if(f->visible && busy(state))f->close_hold=f->config.retain_after_join_polls;
 f->visible=0;f->stop_reason=reason;
 if(busy(state))return ns_controller_stop(f->controller,reason);
 return NS_OK;
}
enum ns_result nss_hide(struct nss_flow *f){return nss_stop(f,NS_PAGE_HIDDEN);}
enum ns_result nss_poll(struct nss_flow *f){
 if(!valid(f))return NS_INVALID;
 struct ns_controller *c=f->controller;ns_u32 state=ns_state(&c->manager);
 if(f->error)return NS_INVALID;
 if(f->visible && f->started_epoch==f->wanted_epoch && f->hooks.heartbeat)f->hooks.heartbeat(f->hooks.context);
 if(busy(state)){
  if(f->visible && f->started_epoch!=f->wanted_epoch){
   (void)ns_controller_stop(c,NS_PAGE_HIDDEN);
   if(f->deferred_epoch!=f->wanted_epoch){f->deferred_epoch=f->wanted_epoch;f->deferred++;}
  }
  /* Diagnostic retention starts only after join. Expiry does not itself
   * grant release: next controller poll obtains fresh queue/timer evidence. */
  if(c->join_verified && f->close_hold){f->close_hold--;f->hold_polls++;return NS_BUSY;}
  state=ns_controller_poll(c);if(state==NS_HELD){f->error=2;return NS_INVALID;}
 }
 if(!busy(state) && f->visible && f->started_epoch!=f->wanted_epoch){
  enum ns_result r=ns_controller_open(c,f->config.roles);
  if(r!=NS_OK){f->error=3;return r;}
  f->started_epoch=f->wanted_epoch;state=ns_state(&c->manager);
 }
 return NS_OK;
}
enum ns_result nss_observe(const struct nss_flow *f,struct nss_snapshot *out){
 if(!valid(f)||!out)return NS_INVALID;
 ns_u32 state=ns_state(&f->controller->manager),view=NSS_HIDDEN;
 if(f->error||state==NS_HELD)view=NSS_HELD;
 else if(f->visible && f->wanted_epoch!=f->started_epoch)view=busy(state)?NSS_WAIT_PRIOR:NSS_STARTING;
 else if(!busy(state)&&ns_session_quiescent(&f->controller->manager))view=NSS_QUIET;
 else if(f->visible)view=state==NS_ACTIVE?NSS_READY:NSS_STARTING;
 else if(busy(state))view=NSS_DRAINING;
 else if(ns_session_quiescent(&f->controller->manager))view=NSS_QUIET;
 struct nss_snapshot s={NS_SDK_ABI,view,state,ns_generation(&f->controller->manager),f->visible,f->wanted_epoch,f->started_epoch,f->deferred,f->close_hold,f->hold_polls,f->stop_reason,f->error,ns_session_quiescent(&f->controller->manager)};
 *out=s;return NS_OK;
}
