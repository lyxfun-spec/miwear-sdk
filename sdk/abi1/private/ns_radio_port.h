#ifndef NS_RADIO_PORT_H
#define NS_RADIO_PORT_H
#include "ns_port.h"
#include "ns_ble_packet.h"
struct ns_borrowed {ns_u32 entry,client,internal,external,pid;};
enum ns_target nsr_target(void);
ns_u32 nsr_guards(void);
ns_u32 nsr_borrow(const struct ns_ui_context*,struct ns_borrowed*);
ns_u32 nsr_live(const struct ns_ui_context*,const struct ns_borrowed*);
ns_u32 nsr_callback_context(const struct ns_ui_context*,const struct ns_borrowed*,ns_u32 *tid);
ns_u32 nsr_rpc(const struct ns_borrowed*,void*,ns_u32 command);
ns_u32 nsr_barrier(const struct ns_ui_context*,const struct ns_borrowed*,ns_u32 sync[10]);
enum ns_result nsr_queue_clear(const struct ns_ui_context*,const struct ns_borrowed*,ns_u32 scan,ns_u32 adv);
enum ns_result nsr_timers_clear(const void *owner,ns_u32 bytes,const ns_u32 *scan_cb,const ns_u32 *adv_cb,ns_u32 scan,ns_u32 adv);
void nsr_report(const ns_u32*,ns_u32);
#endif
