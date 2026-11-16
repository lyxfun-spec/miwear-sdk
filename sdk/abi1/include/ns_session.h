#ifndef NS_SESSION_H
#define NS_SESSION_H
/* C11 freestanding. UI owns transitions; producers use the atomic API only.
 * The manager's storage outlives every producer, including rejected callbacks. */
typedef unsigned int ns_u32;
_Static_assert(sizeof(ns_u32)==4,"32-bit unsigned int required");
enum ns_state { NS_IDLE, NS_STARTING, NS_ACTIVE, NS_STOPPING,
                NS_DRAINING, NS_JOINED, NS_RELEASED, NS_HELD };
enum ns_result { NS_OK, NS_BUSY, NS_STALE, NS_INVALID, NS_EXHAUSTED };
enum ns_action { NS_NONE=0, NS_STOP=1, NS_JOIN=2, NS_RELEASE=4 };
enum ns_reason { NS_USER=1, NS_PAGE_HIDDEN, NS_DEACTIVATE, NS_WORKER_FINISHED,
                 NS_BACKEND_ERROR, NS_PROOF_ERROR, NS_GATE_ERROR };
enum { NS_SCAN=1, NS_ADV=2, NS_CLOSED=0x80000000u,
       NS_WORKER_READY=1, NS_WORKER_DONE=2 };
struct ns_handle { ns_u32 generation; };
struct ns_manager {
    /* Atomic words. Do not access directly from integration code. */
    ns_u32 generation, gate, stop, worker;
    /* UI-only state. */
    ns_u32 state, roles, reason, actions, deactivating;
};
/* Fresh, generation-bound backend attestations. They are not substitutes for
 * executing the firmware-specific checks. Zero-initialize at every UI poll. */
struct ns_evidence {
    ns_u32 generation, terminal_roles, internal_barrier, worker_exited;
    ns_u32 join_ok, release_ok, external_ui, own_queue_clear, own_timers_clear;
    ns_u32 fault;
};
void ns_init(struct ns_manager *m);
enum ns_result ns_begin(struct ns_manager *m, ns_u32 roles, struct ns_handle *out);
enum ns_result ns_request_stop(struct ns_manager *m, struct ns_handle h, ns_u32 reason);
void ns_deactivate(struct ns_manager *m);
enum ns_result ns_worker_ready(struct ns_manager *m, struct ns_handle h);
enum ns_result ns_worker_done(struct ns_manager *m, struct ns_handle h);
ns_u32 ns_should_stop(const struct ns_manager *m, struct ns_handle h);
enum ns_result ns_callback_enter(struct ns_manager *m, struct ns_handle h);
enum ns_result ns_callback_leave(struct ns_manager *m, struct ns_handle h);
/* UI-only, returns each side effect once. JOIN requires observed thread exit.
 * RELEASE authorizes only this session owner, never the manager/code/registry. */
ns_u32 ns_drive(struct ns_manager *m, const struct ns_evidence *e);
void ns_hold(struct ns_manager *m, ns_u32 reason);
ns_u32 ns_state(const struct ns_manager *m);
ns_u32 ns_generation(const struct ns_manager *m);
/* Only session quiescence, NOT a module-unload permission. */
ns_u32 ns_session_quiescent(const struct ns_manager *m);
#endif
