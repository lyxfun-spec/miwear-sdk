#include "g1.h"
#include "g2_fixture.h"
#define O(x) __builtin_offsetof(struct g1,x)
const ns_u32 g2_fixture_test_layout[]={sizeof(struct g2_fixture),__builtin_offsetof(struct g2_fixture,done),__builtin_offsetof(struct g2_fixture,app_error),__builtin_offsetof(struct g2_fixture,ready_events),__builtin_offsetof(struct g2_fixture,closed_events)};
const ns_u32 g1_test_layout[]={sizeof(struct g1),O(tag),O(ui),O(client),O(timer),O(tid),O(actual_tid),O(heartbeat),O(stop),O(error),O(phase),O(done),O(joined),O(exit_proof),O(barrier),O(scan_cb),O(gatt_cb),O(scan_token),O(scan_started),O(scan_stopped),O(peer_found),O(connected),O(disconnected),O(discovered),O(subscribed),O(written),O(read),O(notified),O(handle),O(round),O(complete),O(service_start),O(service_end),O(characteristic),O(callback_tid),O(active),O(late),O(report),O(payload_limit),O(mtu_events),O(mtu_warnings),O(mtu_invalid),O(mtu_zero),O(mtu_first_bad_status),O(mtu_first_bad_value),O(mtu_first_bad_round),O(mtu_first_bad_phase),O(mtu_last_status),O(mtu_last_value),O(attr_queries),O(attr_misses),O(attr_first_miss),O(attr_first_transport),O(attr_first_status),O(attr_first_handle),O(attr_first_round),O(attr_first_phase),O(state),O(tokens),O(config)};

const ns_u32 g4_extra_layout[]={O(runtime),O(bindings),O(retire_guard),O(rpc_active),O(wait_deadline),O(transport_held)};

const ns_u32 g4_fixture_layout[]={__builtin_offsetof(struct g2_fixture,report),__builtin_offsetof(struct g2_fixture,app_error),__builtin_offsetof(struct g2_fixture,done)};
