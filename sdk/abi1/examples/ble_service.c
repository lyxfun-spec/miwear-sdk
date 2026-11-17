#include "ns_ble.h"
/* Put this owner in the resident app host, never in a page's storage. */
struct ble_demo {ns1_ble *radio;ns_u32 received,last_error,quiet;};
enum ns_result ble_demo_begin(struct ble_demo *app){
 enum ns_result r=ns1_ble_open(&app->radio);if(r!=NS_OK)return r;
 const unsigned char ad[]={2,1,6,3,0xff,0x12,0x34};
 return ns1_ble_start(app->radio,NS_SCAN|NS_ADV,ad,sizeof ad);
}
/* App-owned UI tick continues across all internal page switches and exit drain.
 * now_ms comes from the host's monotonic clock. Never block this UI loop. */
enum ns_result ble_demo_tick(struct ble_demo *app,ns_u32 now_ms){
 enum ns_result r=ns1_ble_poll(app->radio,now_ms);
 struct ns1_ble_report report;
 while(ns1_ble_receive(app->radio,&report)==NS_OK){
  /* report is an app-owned copy; parse/filter untrusted AD before use. */
  app->received++;
 }
 struct ns1_ble_snapshot s;
 if(ns1_ble_observe(app->radio,&s)==NS_OK){app->last_error=s.error;app->quiet=s.quiet;}
 return r;
}
enum ns_result ble_demo_frame(struct ble_demo *app,unsigned char value){
 const unsigned char ad[]={2,1,6,3,0xff,0x12,value};
 return ns1_ble_change(app->radio,ad,sizeof ad); /* BUSY: retain locally, retry. */
}
enum ns_result ble_demo_exit(struct ble_demo *app){return ns1_ble_exit(app->radio);}
/* Keep ticking until quiet. HELD means resident resources must remain intact.
 * No firmware headers, addresses, callbacks, tokens or application protocol. */
