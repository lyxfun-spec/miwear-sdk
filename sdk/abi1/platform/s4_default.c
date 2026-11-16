#include "default_shell_policy.h"
#define CALL(a,t) ((t)((a)|1u))
#define APP CALL(0x2c796b60,void*(*)(ns_u32))
#define RECOVERED CALL(0x2c79693c,void*(*)(ns_u32))
#define PAGE CALL(0x2c75ee00,void*(*)(ns_u32))
#define INSTALL CALL(0x2c7a23cc,int(*)(void*,void**,ns_u32))
#define SHOWN CALL(0x2c75c2b0,ns_u32(*)(ns_u32,ns_u32))
#define LABEL CALL(0x2c4fe910,void*(*)(void*))
#define TEXT CALL(0x2c506a80,void(*)(void*,const char*))
#define ALIGN CALL(0x2c4dd3c8,void(*)(void*,ns_u32,int,int))
#define VISIBLE CALL(0x2c4d9e34,ns_u32(*)(void*))
#define FREE CALL(0x2c358260,void(*)(void*))
#define OPEN CALL(0x2c6a74c4,int(*)(const char*,int,...))
#define PRINT CALL(0x2c34c35c,int(*)(int,const char*,...))
#define CLOSE CALL(0x2c6a6c2c,int(*)(int))
#define LAUNCH_ADD CALL(0x2c5862ac,void(*)(ns_u32))
typedef ns_u32 u32;typedef int i32;typedef unsigned short u16;
struct ns6_s4_state {
 struct ns_sdk_page session;
 void *timer,*installed,*event_name,*icon_name;
 u32 app[16],page[29];
 u32 phase,ticks,active,error,held,base_info,base_order,baseline_apps,baseline_pages;
 u32 ui_create,ui_destroy,ui_resume,ui_pause,ui_stop,last_root,native_label;
 u32 rows_removed,record_error,signals_add,signals_remove;
 u32 entry,quiet,last_text,buffer,last_report_pass,report[20];
 char text[2][128];
};
static struct ns6_s4_state s;
static const char appname[]="org.research.s4.ns6";
static u32 word(u32 p){return *(volatile u32*)p;}
static u32 half(u32 p){return *(volatile u16*)p;}
static u32 span(u32 p,u32 n){return nsp_valid((void*)p,n);}
static int page_context(void){return s.session.flow.initialized && nsp_ui_matches(&s.session.ble.ui);}
static i32 ui_create(void *page,void *root,void *data){
 (void)data;if(page!=s.page||!root||!page_context()){s.error=80;return 0;}
 s.ui_create++;s.last_root=(u32)root;s.native_label=(u32)LABEL(root);s.last_text=0;
 if(!s.native_label){s.error=81;return 0;}
 ns6page_text(&s.session,s.text[0]);TEXT((void*)s.native_label,s.text[0]);ALIGN((void*)s.native_label,9,0,0);return 0;
}
static i32 ui_resume(void *page){if(page==s.page){if(!page_context())s.error=82;else{s.ui_resume++;ns5page_resume(&s.session);}}return 0;}
static i32 ui_pause(void *page){if(page==s.page){if(!page_context()){s.error=83;return 0;}s.ui_pause++;ns5page_pause(&s.session);}return 0;}
static i32 ui_stop(void *page){if(page==s.page){if(!page_context()){s.error=84;return 0;}s.ui_stop++;ns5page_pause(&s.session);}return 0;}
static i32 ui_destroy(void *page){if(page==s.page){if(!page_context()){s.error=85;return 0;}s.ui_destroy++;ns5page_pause(&s.session);s.native_label=0;}return 0;}
static char *display_name(void){return "S4 NS6 Native";}
static i32 app_signal(void *app,u32 code){(void)app;(void)code;return 0;}
static i32 page_signal(void *page,u32 code,void *data){
 (void)data;if(page==s.page){if(!page_context()){s.error=86;return 0;}if(code==1)s.signals_add++;else if(code==2)s.signals_remove++;}return 0;
}
/* Frozen P17 v2 bounded row/event/launcher checks are inserted below. No
 * unregister or module withdrawal is exposed in this resident NS6 shell. */
#include "s4_default_checks.inc"
static void snapshot(void){
 struct ns_ble_backend *b=&s.session.ble;u32 *v=s.report;
 s.quiet=s.phase==2 && !s.page[12]&&!s.page[9]&&((unsigned char*)s.page)[0x28]==4 && !SHOWN(0xca0000,3) && !ui_queued();
 u32 mi=0,mo=0,ni=0,no=0;
 if(s.phase==2 && APP(0xca)==s.installed && PAGE(0xca0000)==s.page && launcher_snapshot(&ni,&no,&mi,&mo)&&mi==1&&mo==1)s.entry=7;else s.entry=0;
 u32 pass=ns6page_pass(&s.session,s.ui_create,s.ui_destroy,s.ui_resume,s.ui_pause,s.quiet,s.entry)&&!s.error&&!s.record_error&&s.ui_stop==s.ui_create;
 v[0]=0x4e533652u;v[1]=s.ticks;v[2]=s.ui_create|(s.ui_destroy<<16);v[3]=s.ui_resume|(s.ui_pause<<16);
 v[4]=b->created|(b->released<<16);v[5]=b->scan_start|(b->scan_stop<<16);v[6]=b->adv_start|(b->adv_stop<<16);
 v[7]=b->barriers|(b->joined<<16);v[8]=b->callbacks;v[9]=b->overlaps;v[10]=s.session.flow.deferred;v[11]=s.session.flow.wanted_epoch;
 v[12]=s.error?s.error:(s.record_error?90u+s.record_error:s.session.flow.error);v[13]=b->error;
 v[14]=s.quiet;v[15]=s.entry;v[16]=pass;v[17]=pass?0x4e533650u:0;v[18]=ns_state(&b->controller.manager);v[19]=1;
}
static void page_publish(void){
 int fd=OPEN("/tmp/ns_default_v6_result.txt",0x26,0600);if(fd<0)return;
 PRINT(fd,"NS6");for(u32 i=0;i<20;i++)PRINT(fd," %u",s.report[i]);PRINT(fd,"\n");CLOSE(fd);
}
static void update_text(void){
 if(!s.native_label||!s.page[12]||!s.session.flow.visible)return;
 u32 buf=s.buffer^1u;ns6page_text(&s.session,s.text[buf]);u32 h=2166136261u;
 for(u32 i=0;s.text[buf][i]&&i<126u;i++)h=(h^(unsigned char)s.text[buf][i])*16777619u;
 if(h!=s.last_text){TEXT((void*)s.native_label,s.text[buf]);s.buffer=buf;s.last_text=h;}
}
static void callback(void *timer){
 if(timer!=s.timer)return;s.active=1;s.ticks++;
 if(!s.session.flow.initialized){struct ns_ui_context c;if(!nsp_guards()||!nsr_guards()||!nsp_ui_capture(&c)||ns6shell_begin(&s.session,&c)!=NS_OK)s.error=70;}
 if(!s.error && !page_context())s.error=71;
 if(!s.error && s.phase==0){
  u32 ni,no,mi,mo;
  if(!launcher_snapshot(&ni,&no,&mi,&mo)||mi||mo||APP(0xca)||RECOVERED(0xca)||PAGE(0xca0000))s.error=72;
  else{
   s.base_info=ni;s.base_order=no;s.baseline_apps=word(0x3c202794);s.baseline_pages=word(0x3c202798);
   s.app[2]=(u32)appname;s.app[3]=0x2caad1e5;s.app[4]=0x000800ca;s.app[7]=(u32)display_name;s.app[14]=(u32)app_signal;
   s.page[4]=(u32)"native_ns6";s.page[5]=0xca0000;s.page[13]=(u32)page_signal;
   s.page[19]=(u32)ui_create;s.page[20]=(u32)ui_resume;s.page[23]=(u32)ui_pause;s.page[24]=(u32)ui_stop;s.page[25]=(u32)ui_destroy;
   void *pages[]={s.page};s.held=1;INSTALL(s.app,pages,1);s.installed=APP(0xca);
   if(span((u32)s.installed,64)&&word((u32)s.installed+56)==(u32)app_signal && (half((u32)s.installed+18)&8) && PAGE(0xca0000)==s.page){
    s.event_name=(void*)word((u32)s.installed+8);s.icon_name=(void*)word((u32)s.installed+12);
    if(s.event_name&&s.icon_name)s.phase=1;else s.error=73;
   }else s.error=74;
  }
 }else if(!s.error && s.phase==1){
  if(!pending()){
   u32 ni,no,mi,mo;
   if(!launcher_snapshot(&ni,&no,&mi,&mo))s.error=75;
   else{if(!mi)LAUNCH_ADD(0xca);if(!launcher_snapshot(&ni,&no,&mi,&mo)||mi!=1||mo!=1)s.error=76;else s.phase=2;}
  }
 }
 if(!s.error && s.phase==2){ns5page_poll(&s.session);update_text();if(s.session.flow.error||s.session.ble.error)s.error=77;}
 if(s.error)ns5page_pause(&s.session);
 snapshot();
 if(s.ticks%10u==0 || s.report[16]!=s.last_report_pass || s.ticks<=3 || s.error){page_publish();s.last_report_pass=s.report[16];}
 s.active=0;
}
struct ns6_module {int(*uninit)(void*);void *arg,*exports;u32 nexports;};
static int ns6_uninit(void *arg){(void)arg;return s.timer||s.held||s.active||s.session.ble.owner?-16:0;}
int module_initialize(struct ns6_module *m){
 if(!m)return -1;m->uninit=ns6_uninit;m->arg=&s;m->exports=0;m->nexports=0;
 if(!nsp_guards()||!nsr_guards())s.error=68;else{s.timer=nsp_timer(callback,&s);if(!s.timer)s.error=69;}
 snapshot();page_publish();return 0;
}
