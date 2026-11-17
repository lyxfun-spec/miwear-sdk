#ifndef G1_H
#define G1_H
#include "ns_radio_port.h"
#include "ns_link_v3.h"
#include "ns3_resource.h"
#define g1 ns3_link
#define G1_TAG 0x47340002u
#define G1_RETURN 0x4738a739u
struct g1 {
 ns_u32 tag; struct ns_ui_context ui; struct ns_borrowed client;
 void *timer; ns_u32 tid, actual_tid, heartbeat, last_heartbeat, stale;
 ns_u32 stop, error, phase, done, joined, exit_proof, barrier, ticks;
 ns_u32 scan_cb[4], gatt_cb[26], scan_token, scan_started, scan_stopped;
 ns_u32 peer_found, connected, disconnected, discovered, subscribed, written, read, notified;
 ns_u32 handle, round, complete, service_start, service_end, characteristic, callback_tid;
 ns_u32 callbacks, active, late, payload_limit, rpc_status, gatt_status, unexpected_disconnect;
 ns_u32 mtu_events, mtu_warnings, mtu_invalid, mtu_zero;
 ns_u32 mtu_first_bad_status, mtu_first_bad_value, mtu_first_bad_round, mtu_first_bad_phase;
 ns_u32 mtu_last_status, mtu_last_value;
 ns_u32 attr_queries, attr_misses, attr_first_miss;
 ns_u32 attr_first_transport, attr_first_status, attr_first_handle, attr_first_round, attr_first_phase;
 struct ns3_config config;
 ns_u32 state,connect_request,disconnect_request,close_request,tx_pending,tx_size,tx_request_id;
 unsigned char tx_bytes[20],rx_bytes[20],read_bytes[20];
 ns_u32 rx_pending,rx_size,read_pending,read_size;
 ns_u32 seen_ready,seen_tx,seen_disconnect,seen_error,seen_closed,dispatching;
 ns_u32 tokens[2];
 ns3_runtime*runtime;struct ns3_link*next;
 struct ns3_binding bindings[2],retire_guard;
 ns_u32 slot_generation[2],bind_ready,bind_done,retire_ready,retire_done,retire_result,created_tokens,collected_tokens;
 ns_u32 wait_deadline,rpc_active,rpc_command,rpc_deadline,transport_held;
 ns_u32 wake_sem[8],sem_pending,sem_ready,wake_pending;
 struct {ns_u32 length,id;unsigned char bytes[20];} tx_queue[4];
 struct {ns_u32 length,generation;unsigned char bytes[20];} rx_queue[8];
 ns_u32 tx_write,tx_read,rx_write,rx_read,tx_event_ready,tx_event_id;
 ns_u32 read_cancelled,read_last_status,read_last_handle,read_last_length,read_bad_reason;
 ns_u32 rpc_transport,rpc_service,seen_transport_held,sub_generation,discover_generation,read_generation;
 ns_u32 sync[10]; unsigned char peer[8];
 ns_u32 report[28];
};
_Static_assert(sizeof(struct g1)<=2048,"G1 state overlaps code");
void owned_callback(void*);
int g1_uuid(const unsigned char*, unsigned char);
int g1_ad(const unsigned char*, ns_u32);
int g1_fixed(const unsigned char*,ns_u32,ns_u32,int);
#endif
