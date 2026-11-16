#ifndef G2_FIXTURE_H
#define G2_FIXTURE_H
#include "ns_link_v3.h"
#define G2_TAG 0x47340002u
struct g2_fixture {
 ns3_u32 tag,heartbeat;void *timer;ns3_link *link;ns3_runtime*runtime;
 ns3_u32 cancel,started,last_heartbeat,stale,ticks,done,app_error;
 ns3_u32 read_cancels,test_case,passed,closing,case_closed,destroy_tries,ready_events,read_events,rx_events,tx_events,disconnect_events,closed_events;
 ns3_u32 tx_mask,rx_mask,round_reads,quiet,created,collected,joins,exits,barriers,expected_error,busy,connections;
 struct ns3_snapshot snapshot;struct ns3_runtime_snapshot rs;
 ns3_u32 report[26],diagnostics[22];
};
void owned_callback(void*);
#endif
