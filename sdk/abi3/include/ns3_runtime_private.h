#ifndef NS3_RUNTIME_PRIVATE_H
#define NS3_RUNTIME_PRIVATE_H
#include "g1.h"
#define NS3_RUNTIME_TAG 0x4e335254u
struct ns3_runtime {
 ns_u32 tag;struct ns_ui_context ui;struct ns_borrowed client;
 _Alignas(8) ns_u32 async_words[16];struct ns3_link*head;
 ns_u32 ready,pumping,opened,destroyed,live,bytes,dispatches;
};
void nsg3_signal_worker(struct ns3_link*);
void nsg3_wait_worker(struct ns3_link*);
void nsg3_wake_ui(struct ns3_link*);
void nsg3_enable_timer(struct ns3_link*);
void nsg3_pump(struct ns3_link*);
void nsg3_tick(void*);
#endif
