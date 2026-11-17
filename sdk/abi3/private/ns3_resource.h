#ifndef NS3_RESOURCE_H
#define NS3_RESOURCE_H
#include "ns_radio_port.h"
/* Private contract, never application ABI. Storage and cookie never move while
 * either side may hold a reference. Transport ambiguity always retains it. */
enum ns3_binding_state {N3_EMPTY=0,N3_RESERVED,N3_CREATED,N3_LINKED,N3_REMOTE_GONE,N3_RECLAIMED,N3_UNKNOWN};
struct ns3_binding {
 ns_u32 token,node,remote,client,callbacks,head,state,terminal,ever_attempted;
 ns_u32 transport_status,service_status,guard_fence,barrier_ok;
 void*rpc_context;
 ns_u32(*rpc_call)(void*,const struct ns_borrowed*,void*,ns_u32);
 ns_u32(*fence_call)(void*,const struct ns_ui_context*,const struct ns_borrowed*,ns_u32*);
};
enum ns_result nsg3_create(const struct ns_ui_context*,const struct ns_borrowed*,struct ns3_binding*,const ns_u32*callbacks);
enum ns_result nsg3_bind_ui(const struct ns_ui_context*,const struct ns_borrowed*,struct ns3_binding*);
/* Must begin after terminal callback (or a handle that was never connected).
 * Creates a fresh unconnected service handle to cross the profile mutex. */
enum ns_result nsg3_retire_remote(const struct ns_ui_context*,const struct ns_borrowed*,struct ns3_binding*,struct ns3_binding*guard,ns_u32 sync[10]);
enum ns_result nsg3_queue_clear(const struct ns_ui_context*,const struct ns_borrowed*,ns_u32 a,ns_u32 b,ns_u32 scan);
enum ns_result nsg3_collect_ui(const struct ns_ui_context*,const struct ns_borrowed*,struct ns3_binding*,struct ns3_binding*,ns_u32 scan);
#endif
