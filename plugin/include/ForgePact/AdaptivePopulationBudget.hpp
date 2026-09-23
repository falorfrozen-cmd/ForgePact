#pragma once
#include <ForgePact/PopulationProfile.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace ForgePact {
// Main-thread scheduling. A deadline guides throughput, never authorizes an
// unsafe creator or interrupts an indivisible native call.
class AdaptivePopulationBudget {
public:
    using Clock=uint64_t(*)();
    static constexpr uint64_t kTargetUs=5000000;
    static constexpr uint64_t kMinimumBudgetUs=4000,kMaximumBudgetUs=8000;
    static constexpr unsigned kMaxPacks=32,kMaxCopies=32;
    static uint64_t Micros(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    static AdaptivePopulationBudget& Instance(){static AdaptivePopulationBudget b;return b;}
    explicit AdaptivePopulationBudget(Clock clock=&Micros):m_Now(clock){}
    void BeginPass(uint64_t expectedPacks=0){
        m_Pass=true;m_TargetMissed=false;m_PassStart=m_Now();m_StoppedElapsed=0;m_LastProgress=0;
        m_ExpectedPacks=expectedPacks;m_PassGranted=0;m_PendingPacks=0;m_PendingCopies=0;
        m_BudgetUs=6000;m_Paused=false;
    }
    void PlanPacks(uint64_t count){if(!m_Pass)BeginPass();m_ExpectedPacks=(std::max)(m_ExpectedPacks,count);}
    void ObserveBacklog(uint64_t packs,uint64_t copies){
        m_PendingPacks=packs;m_PendingCopies=copies;
        if(m_Pass && PassElapsedMs()>=kTargetUs/1000 && (packs || copies))m_TargetMissed=true;
    }
    void StopPass(){if(m_Pass)m_StoppedElapsed=m_Now()-m_PassStart;m_Pass=false;}
    uint64_t PassElapsedMs()const{return (m_Pass?m_Now()-m_PassStart:m_StoppedElapsed)/1000;}
    uint64_t LastProgressMs()const{return m_LastProgress/1000;}
    bool TargetExceeded()const{return m_TargetMissed || (m_Pass && PassElapsedMs()>=kTargetUs/1000 && (m_PendingPacks || m_PendingCopies));}
    void BeginFrame(uint64_t frame,bool active){
        if(m_HaveFrame && frame==m_Frame){m_Active=m_Active||active;return;}
        const auto now=m_Now();bool severe=false;
        if(m_HaveFrame && now>=m_LastPresent && m_Active){
            const auto dt=now-m_LastPresent;
            m_LastFrameUs=dt;m_PeakFrameUs=(std::max)(m_PeakFrameUs,dt);
            if(dt>=4000 && dt<=100000)m_Samples[m_SamplePos++%m_Samples.size()]=dt;
            // A fast loading frame is not the normal FPS. The old recent
            // minimum classified healthy 50fps as stalls and starved the queue.
            auto samples=m_Samples;std::sort(samples.begin(),samples.end());
            auto first=std::find_if(samples.begin(),samples.end(),[](uint64_t n){return n!=0;});
            if(first!=samples.end())m_FrameEstimateUs=std::clamp(*(first+(samples.end()-first)/2),uint64_t(8333),uint64_t(50000));
            if(dt>(std::max)(uint64_t(33333),m_FrameEstimateUs*3/2))++m_SlowFrames;
            // One recovery frame only after a severe native burst, never a
            // permanent cooldown caused by ordinary frame jitter or combat.
            severe=dt>100000 && m_ActualUs>kMaximumBudgetUs;
            if(m_Packs && m_ActualUs>m_CopyActualUs){
                const auto estimate=std::clamp((m_ActualUs-m_CopyActualUs)/m_Packs,uint64_t(150),uint64_t(1000));
                m_PackEstimateUs=(m_PackEstimateUs*3+estimate)/4;
            }
        }
        uint64_t wanted=6000;
        if(m_Pass){
            const auto elapsed=now-m_PassStart;
            // Leave a second for native timers and staggered polling instead
            // of waiting until the final frame to demand all remaining work.
            const uint64_t remaining=elapsed+1000000<kTargetUs?kTargetUs-elapsed-1000000:250000;
            const auto expectedLeft=m_ExpectedPacks>m_PassGranted?m_ExpectedPacks-m_PassGranted:0;
            const auto packs=(std::max)(expectedLeft,m_PendingPacks);
            const auto work=packs*m_PackEstimateUs+m_PendingCopies*100;
            wanted=std::clamp(work*m_FrameEstimateUs/remaining,kMinimumBudgetUs,kMaximumBudgetUs);
            if(elapsed>=kTargetUs-1500000)wanted=kMaximumBudgetUs;
        }
        const bool previouslyPaused=m_Paused;m_Paused=severe && !previouslyPaused;
        m_BudgetUs=severe?kMinimumBudgetUs:wanted;
        m_Frame=frame;m_LastPresent=now;m_HaveFrame=true;m_Active=active;
        m_ActualUs=0;m_CopyActualUs=0;m_ReservedUs=0;m_Packs=0;m_CopyCount=0;
    }
    void Activate(){m_Active=true;}
    bool Active()const{return m_Active;}
    uint64_t ChargedUs()const{
        // Pack reservations predict work already included in actual native
        // timing. Adding both charged it twice. Copy work remains separate.
        return (std::max)(m_ActualUs,m_ReservedUs+m_CopyActualUs);
    }
    bool ReservePack(){
        Activate();if(m_Paused || m_Packs>=kMaxPacks)return false;
        if(m_Packs && ChargedUs()+m_PackEstimateUs>m_BudgetUs)return false;
        ++m_Packs;m_ReservedUs+=m_PackEstimateUs;++m_PassGranted;
        if(m_Pass){m_LastProgress=m_Now()-m_PassStart;if(m_LastProgress>=kTargetUs)m_TargetMissed=true;}
        return true;
    }
    bool CanCopy(bool shareWithPacks=false)const{return m_Active && !m_Paused && m_CopyCount<kMaxCopies && ChargedUs()<(shareWithPacks?m_BudgetUs/3:m_BudgetUs);}
    void CopyStarted(){++m_CopyCount;if(m_Pass){m_LastProgress=m_Now()-m_PassStart;if(m_LastProgress>=kTargetUs)m_TargetMissed=true;}}
    void RecordNative(uint64_t elapsed,bool copy=false,int object=-1){
        m_ActualUs+=elapsed;if(copy)m_CopyActualUs+=elapsed;
        if(elapsed>m_PeakNativeUs){m_PeakNativeUs=elapsed;m_PeakNativeObject=object;m_PeakWasCopy=copy;}
        ++m_MeasuredCalls;FP_POP_NATIVE(elapsed);
    }
    uint64_t Now()const{return m_Now();}
    uint64_t BudgetUs()const{return m_BudgetUs;}
    uint64_t ReservedUs()const{return m_ReservedUs;}
    uint64_t LastFrameUs()const{return m_LastFrameUs;}
    uint64_t PeakFrameUs()const{return m_PeakFrameUs;}
    uint64_t PeakNativeUs()const{return m_PeakNativeUs;}
    int PeakNativeObject()const{return m_PeakNativeObject;}
    bool PeakWasDensityCopy()const{return m_PeakWasCopy;}
    uint64_t SlowFrames()const{return m_SlowFrames;}
    uint64_t MeasuredCalls()const{return m_MeasuredCalls;}
private:
    Clock m_Now;
    bool m_Active=false,m_HaveFrame=false,m_Paused=false,m_Pass=false,m_TargetMissed=false;
    uint64_t m_Frame=0,m_LastPresent=0,m_BudgetUs=6000,m_ActualUs=0,m_CopyActualUs=0,m_ReservedUs=0,m_PackEstimateUs=250;
    uint64_t m_LastFrameUs=0,m_PeakFrameUs=0,m_PeakNativeUs=0,m_SlowFrames=0,m_MeasuredCalls=0,m_FrameEstimateUs=16667;
    uint64_t m_PassStart=0,m_StoppedElapsed=0,m_LastProgress=0,m_ExpectedPacks=0,m_PassGranted=0,m_PendingPacks=0,m_PendingCopies=0;
    unsigned m_Packs=0,m_CopyCount=0,m_SamplePos=0;
    std::array<uint64_t,32> m_Samples{};
    int m_PeakNativeObject=-1;
    bool m_PeakWasCopy=false;
};
// Time outermost native creation, including child construction, exactly once.
class PopulationNativeScope {
    static inline thread_local unsigned s_Depth=0;
    bool m_Active=false,m_Outer=false,m_Copy=false;
    uint64_t m_Start=0;
    int m_Object=-1;
public:
    explicit PopulationNativeScope(bool copy=false):m_Copy(copy){
        auto& b=AdaptivePopulationBudget::Instance();m_Active=b.Active();
        if(m_Active){m_Outer=s_Depth++==0;if(m_Outer)m_Start=b.Now();}
    }
    bool Measuring()const{return m_Active && m_Outer;}
    void SetObject(double object){
        if(Measuring() && std::isfinite(object) && object>=0 && object<=INT32_MAX && std::floor(object)==object)
            m_Object=static_cast<int>(object);
    }
    ~PopulationNativeScope(){if(m_Active){--s_Depth;if(m_Outer){auto& b=AdaptivePopulationBudget::Instance();b.RecordNative(b.Now()-m_Start,m_Copy,m_Object);}}}
};
}
