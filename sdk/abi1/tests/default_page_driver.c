#include "default_shell_policy.h"
ns_u32 pg_init(struct ns_sdk_page *p){struct ns_ui_context c;if(!nsp_guards()||!nsr_guards()||!nsp_ui_capture(&c))return 99;return ns6shell_begin(p,&c);}
ns_u32 pg_field(struct ns_sdk_page *p,ns_u32 i){
 struct ns_ble_backend*b=&p->ble;
 ns_u32 v[]={p->flow.error,b->error,ns_state(&b->controller.manager),b->created,b->joined,b->released,b->scan_start,b->scan_stop,b->adv_start,b->adv_stop,b->barriers,b->exit_proofs,b->callbacks,b->overlaps,b->quiet_checks,p->flow.deferred,p->flow.hold_polls,p->flow.wanted_epoch,p->flow.started_epoch,(ns_u32)b->owner};
 return i<20u?v[i]:0;
}
ns_u32 pg_pass(struct ns_sdk_page *p){return ns6page_pass(p,3,3,3,3,1,7);}

ns_u32 pg_begin_case(struct ns_sdk_page *p,ns_u32 which){
 struct ns_ui_context ui;if(!nsp_ui_capture(&ui))return 99;
 struct nss_config cfg={NS_SDK_ABI,3u,0u};
 if(which==0)return ns5page_begin(p,0,&cfg);
 if(which==1)cfg.abi=0;
 if(which==2)ui.tid++;
 if(which==3)cfg.retain_after_join_polls=1001;
 return ns5page_begin(p,&ui,&cfg);
}
