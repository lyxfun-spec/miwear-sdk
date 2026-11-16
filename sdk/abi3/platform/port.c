#include "ns_port.h"
#define P_RD(a) (*(volatile ns_u32*)(a))
#define P_RB(a) (*(volatile unsigned char*)(a))
#define P_FN(a,t) ((t)((a)|1u))
static int pspan(ns_u32 p,ns_u32 n,ns_u32 lo,ns_u32 hi) {
    return !(p&3u) && p>lo && p<=hi && n<=hi-p;
}
static ns_u32 pcritical(void) {
    ns_u32 old,mask=128;__asm__ volatile("mrs %0,basepri":"=r"(old));
    __asm__ volatile("msr basepri_max,%0"::"r"(mask):"memory");return old;
}
static void puncritical(ns_u32 old){__asm__ volatile("msr basepri,%0"::"r"(old):"memory");}
static int pprivileged(void) {
    ns_u32 c,i;__asm__ volatile("mrs %0,control":"=r"(c));__asm__ volatile("mrs %0,ipsr":"=r"(i));
    return !i && !(c&1u);
}
#if NS_TARGET_BAND
#define P_CURRENT 0x200b0364u
#define P_TID 44u
#define P_TCB_BYTES 208u
#define P_ALLOC 0xc3514ecu
#define P_FREE 0xc34cc7cu
#define P_ATTR 0xc348784u
#define P_STACK 0xc3487b6u
#define P_THREAD 0xc350c54u
#define P_JOIN 0xc359764u
#define P_PAUSE 0xc34cb8cu
#define P_TIMER 0xc3abc88u
#define P_TIMER_DELETE 0xc3abdd8u
#include "guards_band11.h"
ns_u32 nsp_valid(const void *p,ns_u32 n) {
    return P_RD(0x200b2330u)==0x3c356b40u && pspan((ns_u32)p,n,P_RD(0x3c356b5cu),P_RD(0x3c356b60u));
}
static int ptcb(ns_u32 p) {
    return P_RD(0x200b0070u)==0x2010d9e0u &&
        pspan(p,P_TCB_BYTES,P_RD(0x2010d9fcu),P_RD(0x2010da00u));
}
#else
#define P_CURRENT 0x3c2abec0u
#define P_TID 12u
#define P_TCB_BYTES 200u
#define P_ALLOC 0x2c35a16cu
#define P_FREE 0x2c358260u
#define P_ATTR 0x2c34aaccu
#define P_STACK 0x2c34aafeu
#define P_THREAD 0x2c351b7cu
#define P_JOIN 0x2c336940u
#define P_PAUSE 0x2c356348u
#define P_TIMER 0x2c4f7488u
#define P_TIMER_DELETE 0x2c4f6f78u
#include "guards_s4.h"
ns_u32 nsp_valid(const void *p,ns_u32 n){return pspan((ns_u32)p,n,0x3c200000u,0x3dbe0000u);}
static int ptcb(ns_u32 p){return nsp_valid((void*)p,P_TCB_BYTES);}
#endif
ns_u32 nsp_guards(void) {
    return port_check_guards();
}
ns_u32 nsp_ui_capture(struct ns_ui_context *c) {
    if(!pprivileged())return 0;
    ns_u32 t=P_RD(P_CURRENT);if(!ptcb(t))return 0;
    c->task=t;c->group=P_RD(t+8u);c->tid=P_RD(t+P_TID);
    if(!c->tid || (c->tid&0x80000000u))return 0;
#if NS_TARGET_BAND
    c->klo=P_RD(0x2010d9fcu);c->khi=P_RD(0x2010da00u);
    if(c->tid!=48u || !pspan(c->group,56u,c->klo,c->khi))return 0;
#else
    c->klo=0x3c200000u;c->khi=0x3dbe0000u;
    if(!nsp_valid((void*)c->group,64u))return 0;
#endif
    return P_RD(P_CURRENT)==t;
}
ns_u32 nsp_ui_matches(const struct ns_ui_context *c) {
    if(!pprivileged() || !ptcb(c->task) || P_RD(P_CURRENT)!=c->task ||
       P_RD(c->task+P_TID)!=c->tid || P_RD(c->task+8u)!=c->group)return 0;
#if NS_TARGET_BAND
    if(c->klo!=P_RD(0x2010d9fcu) || c->khi!=P_RD(0x2010da00u) || c->tid!=48u)return 0;
#endif
    return 1;
}
ns_u32 nsp_worker_identity(const struct ns_ui_context *c,ns_u32 *tid) {
    ns_u32 t=P_RD(P_CURRENT);if(!pprivileged() || !ptcb(t))return 0;
    *tid=P_RD(t+P_TID);
    return *tid && !(*tid&0x80000000u) && *tid!=c->tid && t!=c->task &&
        P_RD(t+8u)==c->group && P_RD(P_CURRENT)==t;
}
enum ns_result nsp_exit_proof(const struct ns_ui_context *c,ns_u32 tid,ns_u32 expected) {
    if(!nsp_ui_matches(c) || !tid || (tid&0x80000000u))return NS_INVALID;
    ns_u32 saved=pcritical();enum ns_result result=NS_INVALID;
#if NS_TARGET_BAND
    (void)expected;
    ns_u32 n=P_RD(0x200b0378u),table=P_RD(0x200ba820u);
    if(n<16u || n>65536u || (n&(n-1u)) || !pspan(table,n*4u,c->klo,c->khi))goto done;
    ns_u32 t=P_RD(table+(tid&(n-1u))*4u);
    if(t && !ptcb(t))goto done;
    if(!t || P_RD(t+P_TID)!=tid){result=NS_OK;goto done;}
    if(P_RD(t+8u)!=c->group)goto done;
    result=(P_RD(t+64u)&0x4000u)?NS_OK:NS_BUSY;
#else
    /* Exact sched_gettcb lookup shape: AP 2c334b00, not a guessed TCB flag.
     * Only absence of this TID plus a completed retained join record permits
     * JOIN. Reading that record is bounded and protected by BASEPRI. */
    ns_u32 n=P_RD(0x3c2aba18u),table=P_RD(0x3c2aba30u);
    if(n<16u || n>65536u || (n&(n-1u)) || !nsp_valid((void*)table,n*4u))goto done;
    ns_u32 t=P_RD(table+(tid&(n-1u))*4u);
    if(t && !ptcb(t))goto done;
    if(t && P_RD(t+12u)==tid){result=NS_BUSY;goto done;}
    ns_u32 node=P_RD(c->group+56u),tail=0,found=0;
    for(ns_u32 i=0;node && i<256u;i++) {
        if(!nsp_valid((void*)node,40u))goto done;
        if(P_RD(node+8u)==tid) {
            if(found || P_RB(node+4u) || P_RB(node+5u) || P_RB(node+6u)!=1u || P_RD(node+36u)!=expected)goto done;
            found=1;
        }
        tail=node;node=P_RD(node);
    }
    if(node || tail!=P_RD(c->group+60u) || !found)goto done;
    result=NS_OK;
#endif
done:puncritical(saved);return result;
}
void *nsp_alloc(ns_u32 n){return P_FN(P_ALLOC,void*(*)(ns_u32))(n);}
void nsp_free(void *p){P_FN(P_FREE,void(*)(void*))(p);}
int nsp_spawn(ns_u32 *tid,void *(*entry)(void*),void *arg) {
    ns_u32 attr[3]={0,0,0};
    if(P_FN(P_ATTR,int(*)(void*))(attr) || P_FN(P_STACK,int(*)(void*,ns_u32))(attr,8192u))return -1;
    if(attr[0]!=0x10064u || attr[1] || attr[2]!=8192u)return -1;
    return P_FN(P_THREAD,int(*)(ns_u32*,void*,void*(*)(void*),void*))(tid,attr,entry,arg);
}
int nsp_join(ns_u32 tid,void **value){return P_FN(P_JOIN,int(*)(ns_u32,void**))(tid,value);}
int nsp_pause(void){return P_FN(P_PAUSE,int(*)(ns_u32))(10000u);}
void *nsp_timer(void(*cb)(void*),void *data){return P_FN(P_TIMER,void*(*)(void(*)(void*),ns_u32,void*))(cb,100u,data);}
void nsp_timer_delete(void *p){P_FN(P_TIMER_DELETE,void(*)(void*))(p);}
void *nsp_timer_data(void *p){return nsp_valid(p,32u)?(void*)P_RD((ns_u32)p+12u):0;}
void nsp_report(const ns_u32 *p,ns_u32 n) {
#if NS_TARGET_BAND
    (void)p;(void)n;
#else
    int fd=P_FN(0x2c6a74c4u,int(*)(const char*,int,...))("/tmp/ns_worker_v2_result.txt",0x26,0600);
    if(fd<0)return;
    P_FN(0x2c34c35cu,int(*)(int,const char*,...))(fd,"NS2");
    for(ns_u32 i=0;i<n;i++)P_FN(0x2c34c35cu,int(*)(int,const char*,...))(fd," %u",p[i]);
    P_FN(0x2c34c35cu,int(*)(int,const char*,...))(fd,"\n");
    P_FN(0x2c6a6c2cu,int(*)(int))(fd);
#endif
}
