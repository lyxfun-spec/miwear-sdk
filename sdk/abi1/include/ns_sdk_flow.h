#ifndef NS_SDK_FLOW_H
#define NS_SDK_FLOW_H
#include "ns_controller.h"
#define NS_SDK_ABI 1u
/* Caller owns zeroed, unpublished storage and keeps controller/hooks context
 * alive. All functions are serialized on the validated UI context. No API
 * here allocates, invokes a radio RPC, or authorizes module/code withdrawal. */
struct nss_config {ns_u32 abi,roles,retain_after_join_polls;};
struct nss_hooks {ns_u32 (*ui_valid)(void*);void (*heartbeat)(void*);void *context;};
struct nss_flow {
 struct ns_controller *controller;
 struct nss_config config;
 struct nss_hooks hooks;
 ns_u32 initialized,visible,wanted_epoch,started_epoch,deferred_epoch,deferred;
 ns_u32 close_hold,hold_polls,error,stop_reason;
};
enum nss_view {NSS_HIDDEN,NSS_WAIT_PRIOR,NSS_STARTING,NSS_READY,NSS_DRAINING,NSS_QUIET,NSS_HELD};
/* Value copy: no owner/token/root/code pointers escape to observers. */
struct nss_snapshot {
 ns_u32 abi,view,state,session_generation,visible,page_epoch,started_epoch;
 ns_u32 deferred,retention_left,retention_polls,reason,error,session_quiet;
};
/* begin binds an already initialized IDLE controller. Invalid calls do not
 * reset storage. A new begin is never a force-reset or retry of held owners. */
enum ns_result nss_begin(struct nss_flow*,struct ns_controller*,const struct nss_config*,const struct nss_hooks*);
/* show is idempotent while visible; new hidden->visible edges get an epoch.
 * It queues intent only: nss_poll is the sole place that opens a backend. */
enum ns_result nss_show(struct nss_flow*);
enum ns_result nss_hide(struct nss_flow*);
/* Explicit stop cancels visibility/reopen intent. A later show may queue a
 * fresh epoch, but cannot bypass a held controller or old drain proofs. */
enum ns_result nss_stop(struct nss_flow*,ns_u32 reason);
/* poll returns NS_OK for a valid step, NS_BUSY during diagnostic retention,
 * NS_INVALID/EXHAUSTED on refusal. Read lifecycle state through observe. */
enum ns_result nss_poll(struct nss_flow*);
enum ns_result nss_observe(const struct nss_flow*,struct nss_snapshot*);
#endif
