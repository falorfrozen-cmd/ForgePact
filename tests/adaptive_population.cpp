#include <ForgePact/AdaptivePopulationBudget.hpp>
#include <ForgePact/DeferredDensityCopies.hpp>
#include <iostream>
#include <stdexcept>
static uint64_t nowUs=0;
static uint64_t clockUs(){return nowUs;}
static void check(bool yes){if(!yes)throw std::runtime_error("adaptive population invariant");}
struct Recipe{double x=0,y=0;};
int main(){
    ForgePact::AdaptivePopulationBudget b(&clockUs);
    b.BeginFrame(0,false);check(!b.Active());
    nowUs=16667;b.BeginFrame(1,true);
    check(b.ReservePack());b.RecordNative(9000);
    check(!b.ReservePack() && !b.CanCopy());
    const auto before=b.BudgetUs();
    nowUs+=120000;b.BeginFrame(2,true);
    check(b.BudgetUs()<before && !b.ReservePack());
    for(int f=3;f<75;++f){nowUs+=16667;b.BeginFrame(f,true);}
    check(b.ReservePack());check(b.BudgetUs()<=8000 && b.BudgetUs()>=4000);
    auto used=b.ReservedUs();b.BeginFrame(74,true);check(b.ReservedUs()==used);
    b.BeginFrame(75,false);check(!b.Active());
    ForgePact::AdaptivePopulationBudget shared(&clockUs);
    shared.BeginFrame(0,true);check(shared.ReservePack());
    shared.RecordNative(9000);check(!shared.CanCopy());
    for(int f=1;f<50;++f){nowUs+=16667;shared.BeginFrame(f,true);}
    check(shared.CanCopy(true));shared.RecordNative(2100);check(!shared.CanCopy(true));
    check(shared.ReservePack()); // copies leave some admission headroom
    auto& live=ForgePact::AdaptivePopulationBudget::Instance();
    live=ForgePact::AdaptivePopulationBudget(&clockUs);live.BeginFrame(1,true);
    const auto calls=live.MeasuredCalls();
    {ForgePact::PopulationNativeScope outer;outer.SetObject(123);nowUs+=200;{ForgePact::PopulationNativeScope inner;inner.SetObject(456);nowUs+=300;}nowUs+=100;}
    check(live.MeasuredCalls()==calls+1 && live.PeakNativeUs()==600);
    check(live.PeakNativeObject()==123 && !live.PeakWasDensityCopy());
    {ForgePact::PopulationNativeScope copy(true);copy.SetObject(789);nowUs+=700;}
    check(live.PeakNativeObject()==789 && live.PeakWasDensityCopy() && live.PeakNativeUs()==700);
    // Reservations overlap native measurements, not additional work.
    ForgePact::AdaptivePopulationBudget overlap(&clockUs);overlap.BeginFrame(0,true);
    check(overlap.ReservePack());overlap.RecordNative(200);
    check(overlap.ChargedUs()==250);
    overlap.RecordNative(100,true);check(overlap.ChargedUs()==350);
    // A missed deadline is explicit and stays visible after the queue drains.
    overlap.BeginPass(20);overlap.ObserveBacklog(20,0);nowUs+=5001000;
    check(overlap.TargetExceeded());overlap.ObserveBacklog(20,0);
    overlap.ObserveBacklog(0,0);overlap.StopPass();check(overlap.TargetExceeded());
    overlap.BeginPass(1);check(!overlap.TargetExceeded() && overlap.PassElapsedMs()==0);
    std::cout<<"budget: inactive baseline, real-work charge, slow-frame backoff, recovery PASS\n";

    ForgePact::DeferredDensityCopies<int,Recipe> q;
    q.Schedule(1,{100,0},3);q.Schedule(2,{10,0},3);q.Schedule(1,{100,0},3);
    check(q.Pending()==6);
    auto first=q.TakeNearest(0,0);check(first && first->key==2 && first->ordinal==1);
    q.Complete(*first);check(q.Pending()==5);
    q.NewZone();check(q.Pending()==0 && q.HasPlan(2));
    q.Schedule(2,{10,0},3);check(q.Pending()==2); // no repeat of completed copy
    while(auto job=q.TakeNearest(0,0))q.Complete(*job);
    q.Schedule(2,{10,0},3);check(q.Pending()==0);
    q.Schedule(1,{100,0},3);check(q.Pending()==3);
    auto old=q.TakeNearest(0,0);q.NewZone();q.Complete(*old);
    q.Schedule(1,{100,0},3);check(q.Pending()==3); // stale completion cannot mark new zone
    q.Reset();check(q.Pending()==0 && !q.HasPlan(1));
    q.Schedule(9,{0,0},1);auto retry=q.TakeNearest(0,0,0);q.Retry(*retry,10);
    check(!q.TakeNearest(0,0,9) && q.TakeNearest(0,0,10));
    std::cout<<"density: nearby first, exact 4x, no reentry growth, transition resume PASS\n";
}
