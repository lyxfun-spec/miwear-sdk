#ifndef NS_LINK_V3_H
#define NS_LINK_V3_H
/* ABI3 development interface. New binaries require their own acceptance.
 * Runtime is a fixed resident service; sessions have explicit safe destroy. */
typedef unsigned int ns3_u32;
typedef struct ns3_link ns3_link;
typedef struct ns3_runtime ns3_runtime;
enum ns3_result {NS3_OK=0,NS3_INVALID=1,NS3_BUSY=2,NS3_SIZE=3,NS3_LIMIT=4,NS3_HELD=5,NS3_CONTEXT=6,NS3_FAILED=7};
enum ns3_state {NS3_IDLE=0,NS3_CONNECTING=1,NS3_READY=2,NS3_DISCONNECTING=3,NS3_DISCONNECTED=4,NS3_CLOSING=5,NS3_CLOSED_HELD=6,NS3_TRANSPORT_HELD=7};
enum ns3_event_type {NS3_EVENT_READY=1,NS3_EVENT_INITIAL_READ=2,NS3_EVENT_RECEIVED=3,NS3_EVENT_WRITE_COMPLETE=4,NS3_EVENT_DISCONNECTED=5,NS3_EVENT_ERROR=6,NS3_EVENT_CLOSED_HELD=7,NS3_EVENT_WRITE_CANCELED=8,NS3_EVENT_TRANSPORT_HELD=9};
struct ns3_event {ns3_u32 type,generation,request_id,error,length;const unsigned char *bytes;};
typedef void (*ns3_observer)(ns3_link*,const struct ns3_event*,void*);
struct ns3_config {ns3_u32 size,abi;unsigned char service_uuid[16],channel_uuid[16];ns3_observer observer;void *user;ns3_runtime*runtime;ns3_u32 connect_timeout_ms,operation_timeout_ms;};
struct ns3_snapshot {ns3_u32 size,abi,state,generation,error,tx_limit,payload_limit,retained_tokens;ns3_u32 report[26],diagnostics[22];ns3_u32 created_tokens,collected_tokens,queued_tx,queued_rx,rpc_active,rpc_command;};
struct ns3_runtime_snapshot {ns3_u32 size,abi,sessions_opened,sessions_destroyed,sessions_live,session_bytes_live,runtime_bytes,dispatches;};
enum ns3_result ns3_runtime_open(ns3_runtime**);
enum ns3_result ns3_runtime_snapshot_get(ns3_runtime*,struct ns3_runtime_snapshot*);
/* Initialize *out to NULL. An occupied slot is BUSY even after CLOSED_HELD. */
enum ns3_result ns3_open(const struct ns3_config*,ns3_link**);
enum ns3_result ns3_connect(ns3_link*);
enum ns3_result ns3_send(ns3_link*,const void*,ns3_u32,ns3_u32);
enum ns3_result ns3_disconnect(ns3_link*);
enum ns3_result ns3_close(ns3_link*);
enum ns3_result ns3_snapshot_get(ns3_link*,struct ns3_snapshot*);
/* Call on the captured UI outside any SDK observer/dispatcher, after close.
 * Refuses if any producer/thread/queue/timer/reference proof is incomplete. */
enum ns3_result ns3_destroy(ns3_link**);
#define NS3_ABI_CANDIDATE 3u
#define NS3_TX_LIMIT 20u
#define NS3_TX_SLOTS 4u
#define NS3_RX_SLOTS 8u
/* send copies before OK. Bounded TX queue, BUSY is backpressure.
 * WRITE_COMPLETE means ATT write response only. RX bytes borrowed until the
 * observer returns. Observer is serial on captured UI owner, may send/close.
 * Eight RX slots of20 bytes; overflow is terminal error, no silent overwrite.
 * Initial characteristic read is automatic for this candidate profile.
 * Borrowed transport cannot be force-canceled safely: deadline reports
 * TRANSPORT_HELD and detaches application observer; SDK data/code remain held
 * until a real reply, producer retirement and real worker join can be proved.
 * This is explicitly not a hard-abort guarantee for shared firmware RPC.
 * Snapshot diagnostics are experimental, not a stable public ABI. */
#endif
