#include "ns_ble_packet.h"
static void put(unsigned char *p,ns_u32 off,ns_u32 v,ns_u32 n) {
    for(ns_u32 i=0;i<n;i++) p[off+i]=(unsigned char)(v>>(8*i));
}
static ns_u32 get(const unsigned char *p,ns_u32 off) {
    ns_u32 v=0; for(ns_u32 i=0;i<4;i++) v|=(ns_u32)p[off+i]<<(8*i); return v;
}
enum ns_result ns_ble_layout(enum ns_target t,struct ns_ble_layout *o) {
    if(!o || (t!=NS_BAND11 && t!=NS_S4)) return NS_INVALID;
    o->packet_bytes=t==NS_BAND11?708:612;
    o->scan_start=t==NS_BAND11?0xed:0xd1; o->scan_stop=t==NS_BAND11?0xee:0xd3;
    o->adv_start=0x43; o->adv_stop=0x44; o->result_data=t==NS_BAND11?12:11;
    o->scan_token_bytes=t==NS_BAND11?616:8; o->adv_token_bytes=t==NS_BAND11?16:12;
    return NS_OK;
}
static enum ns_result prepare(enum ns_target t,unsigned char *p,ns_u32 n) {
    struct ns_ble_layout l;
    if(!p || ns_ble_layout(t,&l)!=NS_OK || n!=l.packet_bytes) return NS_INVALID;
    for(ns_u32 i=0;i<n;i++) p[i]=0;
    return NS_OK;
}
enum ns_result ns_ble_scan(enum ns_target t,unsigned char *p,ns_u32 n,ns_u32 token) {
    if(!token || (token&3u)) return NS_INVALID;
    enum ns_result r=prepare(t,p,n); if(r!=NS_OK) return r;
    put(p,t==NS_BAND11?60:44,token,4);
    if(t==NS_BAND11) { p[68]=0; p[69]=1; } else { p[48]=1; p[49]=1; }
    return NS_OK;
}
enum ns_result ns_ble_adv(enum ns_target t,unsigned char *p,ns_u32 n,ns_u32 token,
                          const unsigned char *ad,ns_u32 len) {
    if(!token || (token&3u) || len>31 || (len && !ad)) return NS_INVALID;
    enum ns_result r=prepare(t,p,n); if(r!=NS_OK) return r;
    put(p,t==NS_BAND11?60:44,token,4);
    ns_u32 data;
    if(t==NS_BAND11) {
        p[68+6]=8; put(p,68+16,320,4); put(p,96,len,2); data=100;
    } else {
        p[48]=8; put(p,64,160,4); put(p,332,len,2); data=76;
    }
    for(ns_u32 i=0;i<len;i++) p[data+i]=ad[i];
    return NS_OK;
}
enum ns_result ns_ble_stop(enum ns_target t,unsigned char *p,ns_u32 n,struct ns_remote remote) {
    if((!remote.lo && !remote.hi) || (t==NS_S4 && remote.hi)) return NS_INVALID;
    enum ns_result r=prepare(t,p,n); if(r!=NS_OK) return r;
    put(p,t==NS_BAND11?60:44,remote.lo,4);
    if(t==NS_BAND11) put(p,64,remote.hi,4);
    return NS_OK;
}
enum ns_result ns_ble_remote(enum ns_target t,const unsigned char *p,ns_u32 n,struct ns_remote *out) {
    struct ns_ble_layout l;
    if(!p || !out || ns_ble_layout(t,&l)!=NS_OK || n!=l.packet_bytes) return NS_INVALID;
    out->lo=get(p,t==NS_BAND11?12:4); out->hi=t==NS_BAND11?get(p,16):0;
    return NS_OK;
}
enum ns_result ns_ble_copy_report(enum ns_target t,const unsigned char *p,ns_u32 n,
                                 unsigned char *out,ns_u32 cap,ns_u32 *len) {
    struct ns_ble_layout l;
    if(!p || !out || !len || ns_ble_layout(t,&l)!=NS_OK || n<l.result_data ||
       p[10]>31 || p[10]>cap || p[10]>n-l.result_data) return NS_INVALID;
    for(ns_u32 i=0;i<p[10];i++) out[i]=p[l.result_data+i];
    *len=p[10]; return NS_OK;
}
