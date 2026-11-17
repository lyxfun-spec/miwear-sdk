#ifndef NS_PORT_H
#define NS_PORT_H
#include "ns_session.h"
struct ns_ui_context { ns_u32 task,group,tid,klo,khi; };
ns_u32 nsp_guards(void);
ns_u32 nsp_ui_capture(struct ns_ui_context*);
ns_u32 nsp_ui_matches(const struct ns_ui_context*);
ns_u32 nsp_worker_identity(const struct ns_ui_context*,ns_u32 *tid);
enum ns_result nsp_exit_proof(const struct ns_ui_context*,ns_u32 tid,ns_u32 expected);
ns_u32 nsp_valid(const void*,ns_u32);
void *nsp_alloc(ns_u32);
void nsp_free(void*);
int nsp_spawn(ns_u32*,void *(*)(void*),void*);
int nsp_join(ns_u32,void**);
int nsp_pause(void);
void *nsp_timer(void (*)(void*),void*);
void nsp_timer_delete(void*);
void *nsp_timer_data(void*);
void nsp_report(const ns_u32*,ns_u32);
#endif
