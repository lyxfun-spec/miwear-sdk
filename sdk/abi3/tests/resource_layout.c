#include "ns3_resource.h"
#define O(x) __builtin_offsetof(struct ns3_binding,x)
const ns_u32 resource_layout[]={sizeof(struct ns3_binding),O(token),O(node),O(remote),O(client),O(callbacks),O(head),O(state),O(terminal),O(ever_attempted),O(transport_status),O(service_status),O(guard_fence),O(barrier_ok)};
enum ns_result resource_force_link(const struct ns_ui_context*u,const struct ns_borrowed*c){return nsg3_queue_clear(u,c,0,0,0);}
