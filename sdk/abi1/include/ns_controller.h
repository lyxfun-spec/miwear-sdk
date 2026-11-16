#ifndef NS_CONTROLLER_H
#define NS_CONTROLLER_H
#include "ns_session.h"
/* All ops execute on the validated UI context. start publishes a pre-bound
 * owner before worker creation; request_stop only signals cancellation and
 * must not perform a blocking radio RPC. inspect returns fresh attestations.
 * join is called only after inspect has proved actual thread exit.
 * release frees only the session owner, not this controller or module code. */
struct ns_backend {
    enum ns_result (*start)(void*,struct ns_manager*,struct ns_handle,ns_u32 roles);
    void (*request_stop)(void*,struct ns_handle);
    enum ns_result (*inspect)(void*,struct ns_handle,struct ns_evidence*);
    enum ns_result (*join)(void*,struct ns_handle);
    enum ns_result (*release)(void*,struct ns_handle);
};
struct ns_controller {
    struct ns_manager manager;
    struct ns_handle handle;
    const struct ns_backend *backend;
    void *context;
    ns_u32 join_verified;
};
enum ns_result ns_controller_init(struct ns_controller*,const struct ns_backend*,void*);
enum ns_result ns_controller_open(struct ns_controller*,ns_u32 roles);
enum ns_result ns_controller_stop(struct ns_controller*,ns_u32 reason);
void ns_controller_deactivate(struct ns_controller*);
ns_u32 ns_controller_poll(struct ns_controller*);
#endif
