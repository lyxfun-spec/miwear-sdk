#ifndef FOUNDATION_COMPONENT_H
#define FOUNDATION_COMPONENT_H
#include "ns_sdk_page.h"
/* Caller allocates zeroed storage, captures the valid target UI identity and
 * owns this object until all callbacks/owners have independently quiesced.
 * This component has no allocation, registry, loader or code-free operation. */
struct foundation_component {struct ns_sdk_page session;};
enum ns_result foundation_begin(struct foundation_component*,const struct ns_ui_context*);
enum ns_result foundation_show(struct foundation_component*);
enum ns_result foundation_hide(struct foundation_component*);
enum ns_result foundation_stop(struct foundation_component*);
enum ns_result foundation_tick(struct foundation_component*);
enum ns_result foundation_observe(const struct foundation_component*,struct nss_snapshot*);
#endif
