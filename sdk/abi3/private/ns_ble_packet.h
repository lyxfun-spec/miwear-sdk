#ifndef NS_BLE_PACKET_H
#define NS_BLE_PACKET_H
#include "ns_session.h"
enum ns_target { NS_BAND11=1, NS_S4=2 };
struct ns_remote { ns_u32 lo,hi; };
struct ns_ble_layout {
    ns_u32 packet_bytes,scan_start,scan_stop,adv_start,adv_stop;
    ns_u32 result_data,scan_token_bytes,adv_token_bytes;
};
/* Pure packet encoding: no allocation, RPC, token dereference, BT deletion,
 * or device memory reads. Token must already be bound to its generation. */
enum ns_result ns_ble_layout(enum ns_target t,struct ns_ble_layout *out);
enum ns_result ns_ble_scan(enum ns_target t,unsigned char *p,ns_u32 bytes,ns_u32 bound_token);
enum ns_result ns_ble_adv(enum ns_target t,unsigned char *p,ns_u32 bytes,
                          ns_u32 bound_token,const unsigned char *ad,ns_u32 len);
enum ns_result ns_ble_stop(enum ns_target t,unsigned char *p,ns_u32 bytes,struct ns_remote remote);
enum ns_result ns_ble_remote(enum ns_target t,const unsigned char *p,ns_u32 bytes,struct ns_remote *out);
/* Copy callback-owned legacy AD bytes before return. Zero is a valid length.
 * Invalid data leaves destination/length untouched. No business parser. */
enum ns_result ns_ble_copy_report(enum ns_target t,const unsigned char *report,ns_u32 report_bytes,
                                 unsigned char *out,ns_u32 capacity,ns_u32 *out_len);
#endif
