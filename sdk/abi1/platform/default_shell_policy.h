#ifndef DEFAULT_SHELL_POLICY_H
#define DEFAULT_SHELL_POLICY_H
#include "ns_sdk_page.h"
static inline enum ns_result ns6shell_begin(struct ns_sdk_page *p,const struct ns_ui_context *ui){
 const struct nss_config cfg={NS_SDK_ABI,NS_SCAN|NS_ADV,0u};return ns5page_begin(p,ui,&cfg);
}
ns_u32 ns6page_pass(const struct ns_sdk_page*,ns_u32,ns_u32,ns_u32,ns_u32,ns_u32,ns_u32);
void ns6page_text(const struct ns_sdk_page*,char[128]);
#endif
