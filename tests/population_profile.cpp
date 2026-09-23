#include <ForgePact/PopulationProfile.hpp>
#include <cassert>
#include <iostream>

#ifndef FORGEPACT_POPULATION_PROFILE
int main() {
    int touched=0;
    FP_POP_SCOPE(++touched);
    FP_POP_BEGIN();
    FP_POP_NATIVE(++touched);
    assert(touched==0);
    std::cout << "profile disabled PASS\n";
}
#else
using namespace ForgePact::PopulationProfile;
static uint64_t nowUs=100, clockCalls=0;
static uint64_t fakeClock(){++clockCalls;return nowUs;}
int main() {
    Recorder r(fakeClock);
    { Scope inactive(r,Metric::DistanceExtra);nowUs+=100; }
    assert(clockCalls==0 && r.Read(Metric::DistanceExtra).calls==0);
    r.Begin(1000);
    // Repeated pass notifications cannot prolong a measurement or reset it.
    nowUs+=50;r.Begin(1000);assert(r.ElapsedUs()==50);
    for(int i=0;i<128;++i){ Scope s(r,Metric::PoolGet);nowUs+=2; }
    auto read=r.Read(Metric::PoolGet);
    assert(read.calls==128 && read.samples==2 && read.sampledUs==4);
    assert(read.period==64); // An estimate, never reported as exact total time.
    { Scope a(r,Metric::DistanceExtra);nowUs+=10; }
    assert(r.Read(Metric::DistanceExtra).sampledUs==10);
    assert(r.Read(Metric::DistanceExtra).period==16);
    r.RecordNative(23);
    assert(r.Read(Metric::NativeCreate).calls==1 && r.Read(Metric::NativeCreate).sampledUs==23);
    r.Frame();nowUs+=100;r.Frame();
    assert(r.FrameCount()==2 && r.LastFrameUs()==100);
    { Scope old(r,Metric::FrameCallback);nowUs+=1000;assert(r.Expire()); }
    assert(!r.Active() && r.Finished());
    auto count=clockCalls;
    { Scope inactive(r,Metric::PoolGet); }
    assert(clockCalls==count);
    r.Begin();assert(!r.Active()); // one bounded run per launch
    const auto elapsed=r.ElapsedUs();nowUs+=2000;assert(r.ElapsedUs()==elapsed);
    std::cout << "profile bounded sampling PASS\n";
}
#endif
