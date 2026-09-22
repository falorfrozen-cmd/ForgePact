#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ForgePact {
// Plans survive leaving a zone; runnable jobs never do. A partially completed
// placement resumes only when the game restores that original placement again.
template<class Key,class Recipe,class Hash=std::hash<Key>> class DeferredDensityCopies {
    struct Plan{unsigned extras=0;uint64_t completed=0,scheduled=0;};
public:
    struct Job{Key key;Recipe recipe;unsigned ordinal;uint64_t generation;uint64_t retryAt=0,sequence=0;};
    bool Schedule(const Key& key,const Recipe& recipe,unsigned extras){
        if(extras>63 || m_Plans.size()>=65536 || Pending()+extras>65536)return false;
        auto [it,added]=m_Plans.try_emplace(key,Plan{extras,0,0});
        auto& p=it->second;
        if(p.scheduled==m_Generation)return true;
        p.scheduled=m_Generation;
        for(unsigned n=1;n<=p.extras;++n)if(!(p.completed&(uint64_t(1)<<(n-1))))AddReady({key,recipe,n,m_Generation,0,m_Sequence++});
        return true;
    }
    bool HasPlan(const Key& key)const{return m_Plans.count(key)!=0;}
    unsigned Target(const Key& key)const{auto it=m_Plans.find(key);return it==m_Plans.end()?0:it->second.extras;}
    size_t Pending()const{return m_Jobs.size()+m_Waiting.size();}
    void NewZone(){++m_Generation;m_Jobs.clear();m_Waiting.clear();m_HaveOrigin=false;m_Sequence=0;}
    void Reset(){NewZone();m_Plans.clear();}
    std::optional<Job> TakeNearest(double x,double y,uint64_t frame=UINT64_MAX){
        if(!std::isfinite(x) || !std::isfinite(y))return std::nullopt;
        // The caller holds one player position throughout its frame batch.
        // Build distances once when that position changes, then select in
        // O(log N), rather than scanning every placement for every copy.
        if(!m_HaveOrigin || x!=m_X || y!=m_Y){
            m_X=x;m_Y=y;m_HaveOrigin=true;
            for(auto& entry:m_Jobs)entry.distance=Distance(entry.job);
            std::make_heap(m_Jobs.begin(),m_Jobs.end(),Farther{});
        }
        while(!m_Waiting.empty() && m_Waiting.front().retryAt<=frame){
            std::pop_heap(m_Waiting.begin(),m_Waiting.end(),Later{});
            Job job=std::move(m_Waiting.back());m_Waiting.pop_back();AddReady(std::move(job));
        }
        while(!m_Jobs.empty()){
            if(!std::isfinite(m_Jobs.front().distance))return std::nullopt;
            std::pop_heap(m_Jobs.begin(),m_Jobs.end(),Farther{});
            Job job=std::move(m_Jobs.back().job);m_Jobs.pop_back();
            if(job.retryAt>frame){const auto retryAt=job.retryAt;Retry(std::move(job),retryAt);continue;}
            return job;
        }
        return std::nullopt;
    }
    void Retry(Job job,uint64_t frame){if(job.generation==m_Generation){job.retryAt=frame;m_Waiting.push_back(std::move(job));std::push_heap(m_Waiting.begin(),m_Waiting.end(),Later{});}}
    void Complete(const Job& job){
        if(job.generation!=m_Generation)return;
        auto it=m_Plans.find(job.key);if(it!=m_Plans.end())it->second.completed|=uint64_t(1)<<(job.ordinal-1);
    }
private:
    struct Ready {Job job;double distance;};
    struct Farther {bool operator()(const Ready& a,const Ready& b)const{
        return a.distance>b.distance || (a.distance==b.distance && a.job.sequence>b.job.sequence);
    }};
    struct Later {bool operator()(const Job& a,const Job& b)const{
        return a.retryAt>b.retryAt || (a.retryAt==b.retryAt && a.sequence>b.sequence);
    }};
    double Distance(const Job& job)const{
        const double dx=job.recipe.x-m_X,dy=job.recipe.y-m_Y,d=dx*dx+dy*dy;
        return std::isfinite(d)?d:std::numeric_limits<double>::infinity();
    }
    void AddReady(Job job){
        const double d=m_HaveOrigin?Distance(job):0;
        m_Jobs.push_back({std::move(job),d});
        if(m_HaveOrigin)std::push_heap(m_Jobs.begin(),m_Jobs.end(),Farther{});
    }
    uint64_t m_Generation=1,m_Sequence=0;
    double m_X=0,m_Y=0;
    bool m_HaveOrigin=false;
    std::unordered_map<Key,Plan,Hash> m_Plans;
    std::vector<Ready> m_Jobs;
    std::vector<Job> m_Waiting;
};
}
