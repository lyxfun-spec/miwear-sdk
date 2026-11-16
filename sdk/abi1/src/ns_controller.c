#include "ns_controller.h"
enum ns_result ns_controller_init(struct ns_controller *c,const struct ns_backend *b,void *context) {
    if(!c || !b || !b->start || !b->request_stop || !b->inspect || !b->join || !b->release) return NS_INVALID;
    ns_init(&c->manager); c->handle.generation=0; c->backend=b; c->context=context; c->join_verified=0;
    return NS_OK;
}
enum ns_result ns_controller_open(struct ns_controller *c,ns_u32 roles) {
    enum ns_result r=ns_begin(&c->manager,roles,&c->handle);
    if(r!=NS_OK) return r;
    c->join_verified=0;
    r=c->backend->start(c->context,&c->manager,c->handle,roles);
    if(r!=NS_OK) {
        c->backend->request_stop(c->context,c->handle);
        ns_hold(&c->manager,NS_BACKEND_ERROR);
    }
    return r;
}
enum ns_result ns_controller_stop(struct ns_controller *c,ns_u32 reason) {
    return ns_request_stop(&c->manager,c->handle,reason);
}
void ns_controller_deactivate(struct ns_controller *c) { ns_deactivate(&c->manager); }
ns_u32 ns_controller_poll(struct ns_controller *c) {
    ns_u32 state=ns_state(&c->manager);
    if(state==NS_IDLE || state==NS_RELEASED || state==NS_HELD) return state;
    struct ns_evidence e={0};e.generation=c->handle.generation;
    enum ns_result r=c->backend->inspect(c->context,c->handle,&e);
    if(r!=NS_OK && r!=NS_BUSY) {
        c->backend->request_stop(c->context,c->handle);
        ns_hold(&c->manager,NS_BACKEND_ERROR);return NS_HELD;
    }
    if(r==NS_BUSY) {
        /* No proof available: still deliver stop, but never join/free. */
        struct ns_evidence empty={0};empty.generation=c->handle.generation;e=empty;
    }
    /* A backend cannot assert join/release on behalf of this controller.
     * The generation it reports is checked by ns_drive, not overwritten. */
    if(e.join_ok || e.release_ok) {
        c->backend->request_stop(c->context,c->handle);
        ns_hold(&c->manager,NS_PROOF_ERROR);return NS_HELD;
    }
    e.join_ok=c->join_verified;
    ns_u32 action=ns_drive(&c->manager,&e);
    if(action&NS_STOP)c->backend->request_stop(c->context,c->handle);
    if(ns_state(&c->manager)==NS_HELD) {
        c->backend->request_stop(c->context,c->handle);return NS_HELD;
    }
    if(action&NS_JOIN) {
        if(c->backend->join(c->context,c->handle)!=NS_OK) {
            ns_hold(&c->manager,NS_BACKEND_ERROR);return NS_HELD;
        }
        c->join_verified=1;
        /* The next UI poll obtains queue/timer/UI snapshots AFTER join. */
    }
    if(action&NS_RELEASE) {
        if(c->backend->release(c->context,c->handle)!=NS_OK) {
            ns_hold(&c->manager,NS_BACKEND_ERROR);return NS_HELD;
        }
        /* No more backend inspection of the freed owner. Acknowledge in the
         * same poll, with gate closed and the already checked final proof. */
        e.release_ok=1;(void)ns_drive(&c->manager,&e);
    }
    return ns_state(&c->manager);
}
