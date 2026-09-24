#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <list>
#include <iterator>
#include <unordered_map>
#include <vector>

namespace ForgePact {
// Stores identities only. Native calls/instance pointers are never retained or replayed.
// A permission is granted only when that live creator next asks through its own event.
class PackAdmissionQueue {
public:
    using Clock = uint64_t (*)();
    static constexpr unsigned kFrameLimit = 32;
    static constexpr uint64_t kFrameBudgetUs = 8000;
    static uint64_t SteadyMicros() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
private:
    struct Entry {
        uint64_t frame;
        std::list<int64_t>::iterator position;
        bool queued;
    };
    std::unordered_map<int64_t,Entry> m_Seen;
    // Most recently requested at the tail. Frame housekeeping only visits
    // expired entries and the first live deadline, not the entire backlog.
    std::list<int64_t> m_Expiry;
    std::vector<int64_t> m_ThisFrame;
    uint64_t m_Frame=0,m_StaleAfter;
    unsigned m_Limit,m_Used=0;
    uint64_t m_Granted=0,m_NativeBirths=0;
    bool m_Deferred=false;
    Clock m_Now;
    uint64_t m_BudgetUs, m_FrameStart=0, m_PassStart=0, m_LastAdmissionUs=0;
    unsigned m_Peak=0;
    uint64_t m_BudgetFrames=0;
    bool m_PassStarted=false, m_BudgetHit=false;
    void Prune() {
        while(!m_Expiry.empty()) {
            auto& entry=m_Seen.at(m_Expiry.front());
            if(m_Frame-entry.frame<=m_StaleAfter)break;
            // A silent creator may be inactive, removed, or have spawned
            // naturally. None of those is a confirmed admission. Retain the
            // identity until a native retry or zone reset resolves it.
            entry.queued=false;
            m_Expiry.pop_front();
        }
    }
public:
    explicit PackAdmissionQueue(unsigned limit=kFrameLimit,uint64_t staleAfter=240,
        uint64_t budgetUs=kFrameBudgetUs,Clock clock=&SteadyMicros)
        :m_StaleAfter(staleAfter),m_Limit(limit),m_Now(clock),m_BudgetUs(budgetUs){m_ThisFrame.reserve(limit);}
    PackAdmissionQueue(const PackAdmissionQueue&)=delete;
    PackAdmissionQueue& operator=(const PackAdmissionQueue&)=delete;
    void Reset() {
        m_Seen.clear();m_Expiry.clear();m_ThisFrame.clear();m_Used=0;m_Deferred=false;m_Granted=0;m_NativeBirths=0;
        m_FrameStart=0;m_PassStart=0;m_LastAdmissionUs=0;m_Peak=0;
        m_BudgetFrames=0;m_PassStarted=false;m_BudgetHit=false;
    }
    void OnFrame(uint64_t frame) {
        if(frame<m_Frame)Reset();
        if(!m_PassStarted) {m_PassStart=m_Now();m_PassStarted=true;}
        m_Frame=frame;m_Used=0;m_ThisFrame.clear();m_Deferred=false;m_BudgetHit=false;Prune();
    }
    bool Request(int64_t id) {return Request(id,[]{return true;});}
    template<class Permit> bool Request(int64_t id,Permit permit) {
        if(id<0)return false;
        if(std::find(m_ThisFrame.begin(),m_ThisFrame.end(),id)!=m_ThisFrame.end())return true;
        auto it=m_Seen.find(id);
        if(it==m_Seen.end() && m_Seen.size()>=65536) {m_Deferred=true;return false;}
        // Only deferred work needs a queue node. Successful fresh admissions
        // use the already-reserved small per-frame array, with no heap churn.
        const auto defer=[&]() {
            if(it==m_Seen.end()) {
                m_Expiry.push_back(id);
                m_Seen.emplace(id,Entry{m_Frame,std::prev(m_Expiry.end()),true});
            } else {
                it->second.frame=m_Frame;
                if(it->second.queued)m_Expiry.splice(m_Expiry.end(),m_Expiry,it->second.position);
                else {
                    m_Expiry.push_back(id);
                    it->second.position=std::prev(m_Expiry.end());
                    it->second.queued=true;
                }
            }
            m_Deferred=true;return false;
        };
        if(m_Used>=m_Limit)return defer();
        const uint64_t now=m_Now();
        if(!m_PassStarted) {m_PassStart=now;m_PassStarted=true;}
        // Includes native work after the previous grant, since that work runs
        // synchronously before a later creator asks. Never replay native events.
        // At least one group can progress; an expensive single call is not preempted.
        if(m_Used && (m_BudgetHit || now-m_FrameStart>=m_BudgetUs)) {
            if(!m_BudgetHit) {++m_BudgetFrames;m_BudgetHit=true;}
            return defer();
        }
        if(!permit())return defer();
        if(!m_Used)m_FrameStart=now;
        if(it!=m_Seen.end()) {
            if(it->second.queued)m_Expiry.erase(it->second.position);
            m_Seen.erase(it);
        }
        m_ThisFrame.push_back(id);++m_Used;++m_Granted;
        m_Peak=(std::max)(m_Peak,m_Used);m_LastAdmissionUs=now-m_PassStart;
        return true;
    }
    bool Tracks(int64_t id)const{return m_Seen.find(id)!=m_Seen.end();}
    bool ObserveNativeBirth(int64_t id){
        const auto it=m_Seen.find(id);if(it==m_Seen.end())return false;
        if(it->second.queued)m_Expiry.erase(it->second.position);
        m_Seen.erase(it);++m_NativeBirths;return true;
    }
    uint64_t NativeBirths()const{return m_NativeBirths;}
    size_t Pending() const {return m_Expiry.size();}
    size_t Unconfirmed() const {return m_Seen.size()-m_Expiry.size();}
    bool HasDeferredWork() const {return m_Deferred || !m_Expiry.empty();}
    uint64_t Granted() const {return m_Granted;}
    unsigned PeakPerFrame() const {return m_Peak;}
    uint64_t BudgetLimitedFrames() const {return m_BudgetFrames;}
    uint64_t LastAdmissionMs() const {return m_LastAdmissionUs/1000;}
};
}
