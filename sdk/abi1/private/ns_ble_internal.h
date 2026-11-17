#ifndef NS_BLE_INTERNAL_H
#define NS_BLE_INTERNAL_H
#include "ns_ble_backend.h"
#define NSB_TAG 0x4e53424fu
struct ns_radio_role {
 ns_u32 token;struct ns_remote remote;
 ns_u32 attempted,status,started,stopped,id,start_rpc,stop_rpc;
};
struct ns_ble_owner {
 ns_u32 tag;struct ns_ble_backend *backend;struct ns_handle handle;
 ns_u32 roles,tid,limit,joined,proof,done,error,actual_tid,barrier;
 ns_u32 callback_tid,callback_mixed,callbacks,active,reports;
 struct ns_borrowed client;
 struct ns_radio_role scan,adv;
 ns_u32 scan_cb[4],adv_cb[3],sync[10];
 unsigned char ad[31];ns_u32 ad_length;
};
#endif
