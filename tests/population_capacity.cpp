// Exercises the production router with small independent native-pool stand-ins.
#include <ForgePact/ProtectedPoolRouter.hpp>
#include <ForgePact/PackAdmissionQueue.hpp>
#include <array>
#include <cstdlib>
#include <new>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ForgePact::ProtectedPool;
static void check(bool b) { if (!b) throw std::runtime_error("population invariant failed"); }
static uint64_t simulatedMicros=0;
static uint64_t simulatedClock() {return simulatedMicros;}
static bool countAllocations=false;
static unsigned allocations=0;
void* operator new(std::size_t n) {
    if(countAllocations)++allocations;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
template<int N> struct Pool {
    static inline std::array<double,8> values{};
    static inline std::array<bool,8> live{};
    static inline unsigned checks=0, scrambled=0, protectedCount=0;
    static inline double integrityResult=0;
    static double init() { live.fill(false); return 0; }
    static double alloc(double v) { for (int i=0;i<8;++i) if(!live[i]) {live[i]=true; values[i]=v; return i;} return -1; }
    static double get(double h) {check(h>=0 && h<8); return values[(int)h];}
    static double set(double h,double v) {check(h>=0 && h<8);values[(int)h]=v;return 0;}
    static double free(double h) {check(h>=0 && h<8);live[(int)h]=false;return 0;}
    static double undef(double h) {return set(h,-77);}
    static double scramble(double) {++scrambled;return 0;}
    static double protect(double) {++protectedCount;return 0;}
    static double verify(double) {++checks;return 0;}
    static double memory(double,double) {++checks;return integrityResult;}
    static Api api() { return {init,alloc,get,set,free,undef,scramble,protect,verify,memory}; }
};
int main() {
    std::cout << std::unitbuf;
    Pool<0>::init(); Pool<1>::init(); Pool<2>::init();
    // Existing vanilla allocations must survive attaching the router.
    check(Pool<0>::alloc(123)==0);
    Router<8,3> r;
    bool fail=false; unsigned loads=0;
    r.Attach(Pool<0>::api(),[&](Api& a) {if(fail) return false; a=loads++==0?Pool<1>::api():Pool<2>::api();a.init();return true;},-77);
    check(r.Get(0)==123 && r.BankCount()==1);
    for(int i=1;i<24;++i) check(r.Allocate(i+0.25)==i);
    check(r.BankCount()==3 && r.OverflowLive()==16);
    for(int i=1;i<24;++i) check(r.Get(i)==i+0.25);
    check(r.Allocate(9)==-1 && r.Failures()==1);
    for(int h:{0,7,8,15,16,23}) {
        r.Set(h,8.75);r.Scramble(h);r.Protect(h);check(r.Get(h)==8.75 && r.Check(h)==0);
        r.Undefine(h);check(r.Get(h)==-77);r.Free(h);r.Free(h);
    }
    for(int h:{0,7,8,15,16,23}) check(r.Allocate(h+0.5)==h);
    check(r.OverflowLive()==16 && r.CheckMemory(0,512)==0);
    check(Pool<0>::checks && Pool<1>::checks && Pool<2>::checks);
    Pool<1>::integrityResult=1;check(r.CheckMemory(0,512)==1);
    Pool<1>::integrityResult=0;check(r.CheckMemory(0,512)==0);
    for(double h:{-1.,24.,1.5,std::numeric_limits<double>::infinity()}) {
        check(r.Get(h)==-77);r.Set(h,0);r.Free(h);check(r.Check(h)==1);
    }
    for(int n=0;n<3;++n) {
        for(int i=0;i<24;++i)r.Free(i);
        check(r.OverflowLive()==0);
        for(int i=0;i<24;++i)check(r.Allocate(i)==i);
        check(r.BankCount()==3);
    }
    r.Reset(); check(r.OverflowLive()==0 && r.Allocate(44)==0 && r.Get(0)==44);
    // Failed growth leaves all original handles and values intact.
    Router<8,3> failed; Pool<0>::init();
    failed.Attach(Pool<0>::api(),[](Api&){return false;},-77);
    for(int i=0;i<8;++i)check(failed.Allocate(i)==i);
    check(!failed.EnsureHeadroom(8) && failed.Allocate(0)==-1 && failed.Get(7)==7);
    std::cout << "router: baseline, overflow, all operations, frees, reset, failure PASS\n";

    // A vanished/not-yet-polling creator must not hold up a ready creator.
    ForgePact::PackAdmissionQueue available(2,240);
    available.OnFrame(1);
    check(available.Request(1) && available.Request(2));
    check(!available.Request(3) && !available.Request(4));
    available.OnFrame(2);
    check(available.Request(4));
    std::cout << "queue: ready tail does not wait for missing head PASS\n";

    // 448 groups is the live report, with deliberately staggered native polls.
    // At 30fps, 60 frames is a two-second admission target, not a game FPS claim.
    ForgePact::PackAdmissionQueue fast(ForgePact::PackAdmissionQueue::kFrameLimit,
        240,ForgePact::PackAdmissionQueue::kFrameBudgetUs,&simulatedClock);
    std::vector<bool> fastDone(448,false);unsigned fastTotal=0;
    for(unsigned frame=0;frame<60;++frame) {
        simulatedMicros=frame*33333ull;
        fast.OnFrame(frame);
        for(unsigned i=0;i<fastDone.size();++i) {
            if(!fastDone[i] && (frame+i)%15==0 && fast.Request(i+1)) {
                fastDone[i]=true;++fastTotal;simulatedMicros+=250;
            }
        }
    }
    check(fastTotal==fastDone.size());
    std::cout << "queue: 448 staggered groups admitted within 60 frames PASS\n";

    ForgePact::PackAdmissionQueue timed(32,240,8000,&simulatedClock);
    timed.OnFrame(1);
    for(int id=1;id<=4;++id) {check(timed.Request(id));simulatedMicros+=2000;}
    check(!timed.Request(5) && !timed.Request(6));
    check(timed.BudgetLimitedFrames()==1 && timed.PeakPerFrame()==4);
    check(timed.Request(4)); // repeated permission does not charge twice
    timed.OnFrame(2);check(timed.Request(6)); // 5 need not poll first
    simulatedMicros+=12000;check(!timed.Request(7));
    check(timed.BudgetLimitedFrames()==2);
    timed.Reset();timed.OnFrame(3);check(timed.Request(7));
    check(timed.Granted()==1 && timed.BudgetLimitedFrames()==0);
    check(!timed.Request(-1));
    std::cout << "queue: elapsed native work yields, repeats, resume, reset PASS\n";

    // More than the last observed zone, all ready immediately: the hard cap
    // still applies even if a native event completes very cheaply.
    ForgePact::PackAdmissionQueue capped(32,240,8000,&simulatedClock);
    capped.OnFrame(1);
    for(int id=1;id<=32;++id)check(capped.Request(id));
    check(!capped.Request(33) && capped.PeakPerFrame()==32);
    capped.OnFrame(2);check(capped.Request(33));

    // Every eligible group is admitted, including a tail taking >900 frames.
    ForgePact::PackAdmissionQueue queue(2,240);
    std::vector<bool> done(2100,false); unsigned total=0;
    for(unsigned frame=1;frame<2500 && total<done.size();++frame) {
        queue.OnFrame(frame);unsigned admitted=0;
        for(unsigned i=0;i<done.size();++i)if(!done[i] && queue.Request(i+1)) {
            check(queue.Request(i+1)); // same creator, same frame: same permission
            done[i]=true;++admitted;++total;
        }
        check(admitted<=2);
        if(frame==901)check(queue.Pending()>0);
    }
    check(total==done.size());
    queue.Reset(); queue.OnFrame(1);check(queue.Request(1));check(queue.Request(2));
    check(!queue.Request(3));check(!queue.Request(4)); // 3 subsequently disappears
    for(unsigned frame=2;frame<245;++frame) {queue.OnFrame(frame);if(queue.Request(4)) break;}
    check(queue.Granted()>2); // vanished head must not block a live tail forever
    queue.Reset();queue.OnFrame(2);check(queue.Request(99));check(queue.Pending()==0);
    std::cout << "queue: bounded, complete, stale entries, reset PASS\n";

    // Live replay: 71 denied groups stop polling. Expiry is NOT proof that
    // they spawned. Keep their identities distinguishable from a drained pass.
    ForgePact::PackAdmissionQueue silent(1,240,8000,&simulatedClock);
    silent.OnFrame(1);check(silent.Request(1));
    for(int id=2;id<=72;++id)check(!silent.Request(id));
    for(unsigned frame=2;frame<=242;++frame)silent.OnFrame(frame);
    check(silent.Pending()==0 && silent.Unconfirmed()==71 && silent.Granted()==1);
    // A later native check can still be granted; confirmation is not counted
    // twice and never grants a missing head permission on another caller.
    check(silent.Request(72));
    check(silent.Unconfirmed()==70 && silent.Granted()==2);
    silent.OnFrame(243);check(!silent.Request(2,[]{return false;}));
    check(silent.Pending()==1 && silent.Unconfirmed()==69);
    silent.OnFrame(483);check(silent.Pending()==1);
    silent.OnFrame(484);check(silent.Pending()==0 && silent.Unconfirmed()==70);
    silent.Reset();check(silent.Pending()==0 && silent.Unconfirmed()==0);
    std::cout << "queue: silent groups remain unconfirmed and late callers resume PASS\n";

    // Refreshing one denied caller must move only its expiry. The previously
    // younger sibling then expires first; a stale FIFO timestamp cannot
    // discard the refreshed caller.
    ForgePact::PackAdmissionQueue refreshed(0,4,8000,&simulatedClock);
    refreshed.OnFrame(1);check(!refreshed.Request(1));
    refreshed.OnFrame(2);check(!refreshed.Request(2));
    refreshed.OnFrame(5);check(!refreshed.Request(1));
    refreshed.OnFrame(7);check(refreshed.Pending()==1 && refreshed.Unconfirmed()==1);
    refreshed.OnFrame(9);check(refreshed.Pending()==1);
    refreshed.OnFrame(10);check(refreshed.Pending()==0 && refreshed.Unconfirmed()==2);
    std::cout << "queue: refreshed deadlines and bounded maintenance PASS\n";

    // Admitting immediately-ready packs should not allocate temporary queue
    // nodes merely to destroy them again in the same Request call.
    ForgePact::PackAdmissionQueue immediate(32,240,8000,&simulatedClock);
    immediate.OnFrame(1);allocations=0;countAllocations=true;
    for(int id=1;id<=32;++id)check(immediate.Request(id));
    countAllocations=false;
    std::cout << "immediate admission allocations=" << allocations << "\n";
    check(allocations==0);
    std::cout << "queue: immediate admission allocates no nodes PASS\n";

    ForgePact::PackAdmissionQueue natural(0,2,8000,&simulatedClock);
    natural.OnFrame(1);check(!natural.Request(10));check(!natural.Request(20));
    check(natural.ObserveNativeBirth(10));
    check(natural.Pending()==1 && natural.Granted()==0 && natural.NativeBirths()==1);
    check(!natural.ObserveNativeBirth(10) && !natural.ObserveNativeBirth(999));
    natural.OnFrame(4);check(natural.Unconfirmed()==1);
    check(natural.ObserveNativeBirth(20));
    check(natural.Unconfirmed()==0 && natural.NativeBirths()==2 && natural.Granted()==0);
    natural.Reset();check(natural.NativeBirths()==0 && !natural.ObserveNativeBirth(20));
    std::cout << "queue: natural births resolve only observed waiting identities PASS\n";
}
