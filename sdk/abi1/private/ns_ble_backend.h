#ifndef NS_BLE_BACKEND_H
#define NS_BLE_BACKEND_H
#include "ns_controller.h"
#include "ns_radio_port.h"
#include "ns_ble.h"
struct ns_ble_owner;
/* Zero-filled, unpublished UI-owned storage before first init. No reset while
 * a generation or HELD owner exists. This backend only BORROWS a local client. */
struct ns_ble_backend {
 struct ns_controller controller;struct ns_backend ops;struct ns_ui_context ui;
 struct ns_ble_owner *owner;
 ns_u32 limit,heartbeat,created,joined,released,scan_start,scan_stop,adv_start,adv_stop;
 ns_u32 barriers,exit_proofs,callbacks,reports,overlaps,quiet_checks,error,last_tid,last_mode;
 ns_u32 custom_ad,ad_length,receive_enabled,rx_head,rx_tail,rx_dropped;
 unsigned char ad[31];
 struct ns1_ble_report rx[NS1_BLE_RX_SLOTS];
};
/* Single validated callback producer, UI consumer; never reset live indices. */
void nsb_receive_copy(struct ns_ble_backend*,ns_u32,const unsigned char*,ns_u32);
enum ns_result nsb_init(struct ns_ble_backend*,const struct ns_ui_context*);
void nsb_heartbeat(struct ns_ble_backend*);
void *nsb_worker(void*);
#endif
