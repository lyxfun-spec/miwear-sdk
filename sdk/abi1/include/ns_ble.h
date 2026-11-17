#ifndef NS_BLE_H
#define NS_BLE_H
#include "ns_session.h"
#define NS1_BLE_AD_LIMIT 31u
#define NS1_BLE_RX_SLOTS 8u
#define NS1_BLE_CHANGE_MS 1000u
typedef struct ns1_ble ns1_ble;
struct ns1_ble_report { ns_u32 generation,length; unsigned char bytes[31]; };
struct ns1_ble_snapshot {
 ns_u32 state,generation,error,pending,queued_rx,dropped_rx,quiet,exiting;
};
/* Open ONCE per resident app owner and reuse it; exclusive radio use is a host
 * precondition, not cross-module arbitration. *out must be NULL.
 * All calls serialized on the captured foreground UI.
 * No callbacks or application pointers are retained. Code/service stay resident
 * until reboot, including after exit. Do not run another radio host alongside. */
enum ns_result ns1_ble_open(ns1_ble **out);
/* Raw legacy advertising AD structures, 1..31 bytes for NS_ADV; scan-only
 * requires NULL,0. Copied before OK. No fragmentation or delivery guarantee. */
enum ns_result ns1_ble_start(ns1_ble*,ns_u32 roles,const void*,ns_u32 length);
/* One pending frame. ACTIVE + NS_ADV only; BUSY means retry later, no copy.
 * Stops BOTH roles and restarts only after full release and >=1000ms since
 * prior start. Thus changing frames has a scan/advertising gap; no hot RPC. */
enum ns_result ns1_ble_change(ns1_ble*,const void*,ns_u32 length);
/* Application supplies monotonic uint32 milliseconds (wrap allowed); call
 * regularly from an app-owned UI timer, including while stopping. First poll
 * establishes the clock; gaps/backward steps >=2^31 ms are invalid. */
enum ns_result ns1_ble_poll(ns1_ble*,ns_u32 now_ms);
/* Value copy; empty is BUSY, overflow drops NEW reports and increments dropped.
 * Reports retain generation; duplicates/untrusted raw AD are application data. */
enum ns_result ns1_ble_receive(ns1_ble*,struct ns1_ble_report*);
enum ns_result ns1_ble_observe(ns1_ble*,struct ns1_ble_snapshot*);
/* stop cancels pending frames, permits later start after quiet. exit is final
 * for this service; poll until quiet or HELD, never force free/reset on HELD.
 * Incomplete drain after 10s of polled clock becomes HELD (no forced abort).
 * Page switches call neither stop nor exit. No sleep/suspend guarantee. */
enum ns_result ns1_ble_stop(ns1_ble*);
enum ns_result ns1_ble_exit(ns1_ble*);
#endif
