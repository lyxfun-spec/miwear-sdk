#ifndef NS_SDK_PAGE_H
#define NS_SDK_PAGE_H
#include "ns_ble_backend.h"
#include "ns_sdk_flow.h"
struct ns_sdk_page {struct ns_ble_backend ble;struct nss_flow flow;};
/* Platform adapter: caller provides zeroed unpublished storage. Hooks validate
 * the original UI identity before generic state changes; BLE ownership and
 * firmware-specific proofs remain in the frozen backend/ports. */
enum ns_result ns5page_begin(struct ns_sdk_page*,const struct ns_ui_context*,const struct nss_config*);
enum ns_result ns5page_resume(struct ns_sdk_page*);
enum ns_result ns5page_pause(struct ns_sdk_page*);
enum ns_result ns5page_stop(struct ns_sdk_page*,ns_u32);
enum ns_result ns5page_poll(struct ns_sdk_page*);
enum ns_result ns5page_observe(const struct ns_sdk_page*,struct nss_snapshot*);
/* The following two functions belong to the engineering shell, not a release
 * or business-app API. Their PASS requires three complete duplex cycles. */
ns_u32 ns5page_pass(const struct ns_sdk_page*,ns_u32,ns_u32,ns_u32,ns_u32,ns_u32,ns_u32);
void ns5page_text(const struct ns_sdk_page*,char out[128]);
#endif
