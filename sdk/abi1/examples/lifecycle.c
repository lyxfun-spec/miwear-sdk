#include "ns_sdk_flow.h"

/* Public ABI1 integration sketch. Storage must be fresh and unpublished.
 * The caller supplies an audited backend and UI hooks, owns their lifetime,
 * and drives poll on that UI. A quiet session never authorizes code unload. */
struct lifecycle_example {
    struct ns_controller controller;
    struct nss_flow flow;
};

enum ns_result example_begin(struct lifecycle_example *app,
                            const struct ns_backend *backend, void *context,
                            const struct nss_hooks *hooks) {
    if (!app) return NS_INVALID;
    enum ns_result result = ns_controller_init(&app->controller, backend, context);
    if (result != NS_OK) return result;
    const struct nss_config config = {NS_SDK_ABI, NS_SCAN | NS_ADV, 0u};
    return nss_begin(&app->flow, &app->controller, &config, hooks);
}

enum ns_result example_show(struct lifecycle_example *app) {
    return app ? nss_show(&app->flow) : NS_INVALID;
}

enum ns_result example_hide(struct lifecycle_example *app) {
    return app ? nss_hide(&app->flow) : NS_INVALID;
}

enum ns_result example_poll(struct lifecycle_example *app) {
    return app ? nss_poll(&app->flow) : NS_INVALID;
}
