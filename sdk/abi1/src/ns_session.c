#include "ns_session.h"
static ns_u32 load(const ns_u32 *p) { return __atomic_load_n(p,__ATOMIC_ACQUIRE); }
static void store(ns_u32 *p,ns_u32 n) { __atomic_store_n(p,n,__ATOMIC_RELEASE); }
static ns_u32 current(const struct ns_manager *m,struct ns_handle h) {
    return h.generation && h.generation==load(&m->generation);
}
void ns_init(struct ns_manager *m) {
    /* Initialization only before publishing m to any other context. */
    store(&m->generation,0); store(&m->gate,NS_CLOSED);
    store(&m->stop,0); store(&m->worker,0);
    m->state=NS_IDLE; m->roles=0; m->reason=0;
    m->actions=0; m->deactivating=0;
}
enum ns_result ns_begin(struct ns_manager *m,ns_u32 roles,struct ns_handle *out) {
    if(!out || roles & ~(NS_SCAN|NS_ADV)) return NS_INVALID;
    if(m->deactivating || (m->state!=NS_IDLE && m->state!=NS_RELEASED)) return NS_BUSY;
    ns_u32 old=load(&m->generation);
    if(old==0xffffffffu) return NS_EXHAUSTED; /* Never reuse a generation. */
    if(load(&m->gate)!=NS_CLOSED) return NS_BUSY;
    store(&m->stop,0); store(&m->worker,0);
    m->roles=roles; m->reason=0; m->actions=0; m->state=NS_STARTING;
    store(&m->generation,old+1); store(&m->gate,0);
    out->generation=old+1; return NS_OK;
}
enum ns_result ns_request_stop(struct ns_manager *m,struct ns_handle h,ns_u32 reason) {
    /* UI-only. Producers signal worker completion; they do not write reason. */
    if(!current(m,h)) return NS_STALE;
    if(!reason) return NS_INVALID;
    if(m->state==NS_RELEASED || m->state==NS_IDLE) return NS_STALE;
    if(!m->reason) m->reason=reason;
    store(&m->stop,1); return NS_OK;
}
void ns_deactivate(struct ns_manager *m) {
    m->deactivating=1;
    if(m->state!=NS_IDLE && m->state!=NS_RELEASED)
        (void)ns_request_stop(m,(struct ns_handle){load(&m->generation)},NS_DEACTIVATE);
}
enum ns_result ns_callback_enter(struct ns_manager *m,struct ns_handle h) {
    if(!current(m,h)) return NS_STALE;
    ns_u32 v=load(&m->gate);
    for(;;) {
        if(v&NS_CLOSED) return NS_BUSY;
        if(v==NS_CLOSED-1u) return NS_BUSY;
        if(__atomic_compare_exchange_n(&m->gate,&v,v+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) break;
    }
    /* A delayed old enter may race a close/reopen. It must not enter the new
     * generation, and must relinquish the reference it just acquired. */
    if(!current(m,h)) {
        __atomic_fetch_sub(&m->gate,1,__ATOMIC_ACQ_REL); return NS_STALE;
    }
    return NS_OK;
}
enum ns_result ns_callback_leave(struct ns_manager *m,struct ns_handle h) {
    if(!current(m,h)) return NS_STALE;
    ns_u32 v=load(&m->gate);
    for(;;) {
        if(!(v&~NS_CLOSED)) return NS_INVALID;
        if(__atomic_compare_exchange_n(&m->gate,&v,v-1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return NS_OK;
    }
}
static enum ns_result publish(struct ns_manager *m,struct ns_handle h,ns_u32 fact) {
    enum ns_result r=ns_callback_enter(m,h);
    if(r!=NS_OK) return r;
    __atomic_fetch_or(&m->worker,fact,__ATOMIC_ACQ_REL);
    return ns_callback_leave(m,h);
}
enum ns_result ns_worker_ready(struct ns_manager *m,struct ns_handle h) {
    return publish(m,h,NS_WORKER_READY);
}
enum ns_result ns_worker_done(struct ns_manager *m,struct ns_handle h) {
    return publish(m,h,NS_WORKER_DONE);
}
ns_u32 ns_should_stop(const struct ns_manager *m,struct ns_handle h) {
    return !current(m,h) || load(&m->stop);
}
void ns_hold(struct ns_manager *m,ns_u32 reason) {
    if(m->state==NS_IDLE || m->state==NS_RELEASED) return;
    store(&m->stop,1); m->reason=reason?reason:NS_BACKEND_ERROR; m->state=NS_HELD;
    /* Keep callbacks available for late stop-final notifications. No release. */
}
ns_u32 ns_drive(struct ns_manager *m,const struct ns_evidence *e) {
    ns_u32 action=0;
    if(m->state==NS_IDLE || m->state==NS_RELEASED || m->state==NS_HELD) return NS_NONE;
    /* Reject stale evidence without modifying this generation. */
    if(!e || e->generation!=load(&m->generation)) return NS_NONE;
    if(e->fault) { ns_hold(m,NS_BACKEND_ERROR); return NS_NONE; }
    if((e->terminal_roles & ~m->roles) ||
       (e->join_ok && !(m->actions&NS_JOIN)) ||
       (e->release_ok && !(m->actions&NS_RELEASE))) {
        ns_hold(m,NS_PROOF_ERROR); return NS_NONE;
    }
    ns_u32 w=load(&m->worker);
    if((w&NS_WORKER_DONE) && !load(&m->stop))
        (void)ns_request_stop(m,(struct ns_handle){e->generation},NS_WORKER_FINISHED);
    if(load(&m->stop) && m->state<NS_STOPPING) m->state=NS_STOPPING;
    if(m->state==NS_STARTING && (w&NS_WORKER_READY)) m->state=NS_ACTIVE;
    if(m->state==NS_STOPPING) {
        if(!(m->actions&NS_STOP)) { m->actions|=NS_STOP; action|=NS_STOP; }
        if((w&NS_WORKER_DONE) && e->worker_exited && e->internal_barrier &&
           e->terminal_roles==m->roles) m->state=NS_DRAINING;
    }
    if(m->state==NS_DRAINING) {
        if(!(m->actions&NS_JOIN)) { m->actions|=NS_JOIN; action|=NS_JOIN; }
        if(e->join_ok) m->state=NS_JOINED;
    }
    if(m->state==NS_JOINED) {
        /* Fresh snapshots must agree in THIS poll, after successful join.
         * A callback count of zero alone never grants release. */
        if(!e->worker_exited || !e->internal_barrier || e->terminal_roles!=m->roles ||
           !e->external_ui || !e->own_queue_clear || !e->own_timers_clear) return action;
        __atomic_fetch_or(&m->gate,NS_CLOSED,__ATOMIC_ACQ_REL);
        if(load(&m->gate)!=NS_CLOSED) return action;
        if(!(m->actions&NS_RELEASE)) { m->actions|=NS_RELEASE; action|=NS_RELEASE; }
        if(e->release_ok) m->state=NS_RELEASED;
    }
    return action;
}
ns_u32 ns_state(const struct ns_manager *m) { return m->state; } /* UI only */
ns_u32 ns_generation(const struct ns_manager *m) { return load(&m->generation); }
ns_u32 ns_session_quiescent(const struct ns_manager *m) {
    return (m->state==NS_IDLE || m->state==NS_RELEASED) && load(&m->gate)==NS_CLOSED;
}
