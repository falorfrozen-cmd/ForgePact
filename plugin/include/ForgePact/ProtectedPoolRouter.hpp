#pragma once
// Original routing logic. Each bank retains the native library's own operations.
#include <array>
#include <atomic>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace ForgePact::ProtectedPool {
using Fn0 = double(*)();
using Fn1 = double(*)(double);
using Fn2 = double(*)(double,double);
// The supported native check asks VirtualQuery for a 4096-byte buffer although
// its own frame reserves less. Give it committed caller stack space; preserve
// its real result. A shallow standalone C++ caller otherwise reports failure
// for correctly protected pages (ERROR_NOACCESS), unlike a deep game caller.
#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
inline double NativeMemoryCheck(Fn2 check,double begin,double count) {
    volatile unsigned char space[8192];
    space[0]=0;space[8191]=0;
    const double result=check(begin,count);
    (void)space[0];(void)space[8191];
    return result;
}
struct Api {
    Fn0 init{};
    Fn1 alloc{}, get{};
    Fn2 set{};
    Fn1 free{}, undef{}, scramble{}, protect{}, check{};
    Fn2 memory{};
    bool Complete() const {return init && alloc && get && set && free && undef && scramble && protect && check && memory;}
};

template<size_t Capacity=262144, size_t MaxBanks=16> class Router {
    static_assert(Capacity>0 && MaxBanks>0 && Capacity*MaxBanks < 2147483647);
    std::array<Api,MaxBanks> m_Api{};
    std::array<std::bitset<Capacity>,MaxBanks> m_Live{};
    std::array<size_t,MaxBanks> m_Used{};
    std::atomic<size_t> m_Count{0}, m_Failures{0}, m_Invalid{0}, m_OverflowLive{0};
    std::recursive_mutex m_Mutation;
    std::function<bool(Api&)> m_Grow;
    double m_Undefined{};
    bool AddBank() {
        const auto n=m_Count.load(); if(n>=MaxBanks) return false;
        Api api;
        try {if(!m_Grow || !m_Grow(api) || !api.Complete()) return false;} catch(...) {return false;}
        m_Api[n]=api; m_Used[n]=0; m_Live[n].reset();
        m_Count.store(n+1,std::memory_order_release); return true;
    }
    bool Split(double h,size_t& bank,size_t& local) {
        if(!std::isfinite(h) || h<0 || h>=double(Capacity*MaxBanks) || std::floor(h)!=h) {++m_Invalid;return false;}
        const auto v=static_cast<size_t>(h);bank=v/Capacity;local=v%Capacity;
        if(bank>=m_Count.load(std::memory_order_acquire)) {++m_Invalid;return false;}
        return true;
    }
    double Unary(Fn1 Api::*member,double h,double invalidResult) {
        size_t b,i;if(!Split(h,b,i)) return invalidResult;
        return (m_Api[b].*member)(double(i));
    }
public:
    // Attaching deliberately does NOT initialize bank zero: it may own live game values.
    void Attach(Api original,std::function<bool(Api&)> grow,double undefinedValue) {
        std::lock_guard lock(m_Mutation);
        if(m_Count.load()!=0 || !original.Complete()) throw std::logic_error("Invalid native pool attachment");
        m_Api[0]=original;m_Grow=std::move(grow);m_Undefined=undefinedValue;m_Count.store(1,std::memory_order_release);
    }
    double Allocate(double value) {
        std::lock_guard lock(m_Mutation);
        for(size_t b=0;;++b) {
            if(b==m_Count.load() && !AddBank()) {++m_Failures;return -1;}
            // Full banks return -1 cheaply; native frees rewind their own cursor.
            const double local=m_Api[b].alloc(value);
            if(local==-1) continue;
            if(!std::isfinite(local) || local<0 || local>=Capacity || std::floor(local)!=local) {++m_Failures;return -1;}
            if(b) {
                const auto slot=static_cast<size_t>(local);
                if(m_Live[b].test(slot)) {++m_Failures;return -1;}
                m_Live[b].set(slot);++m_Used[b];++m_OverflowLive;
            }
            return double(b*Capacity)+local;
        }
    }
    double Get(double h) {return Unary(&Api::get,h,m_Undefined);}
    double Set(double h,double value) {size_t b,i;return Split(h,b,i)?m_Api[b].set(double(i),value):0;}
    double Undefine(double h) {return Unary(&Api::undef,h,0);}
    double Scramble(double h) {return Unary(&Api::scramble,h,0);}
    double Protect(double h) {return Unary(&Api::protect,h,0);}
    double Check(double h) {return Unary(&Api::check,h,1);}
    double Free(double h) {
        std::lock_guard lock(m_Mutation);size_t b,i;if(!Split(h,b,i)) return 0;
        const double result=m_Api[b].free(double(i));
        if(b && m_Live[b].test(i)) {m_Live[b].reset(i);--m_Used[b];--m_OverflowLive;}
        return result;
    }
    double CheckMemory(double begin,double count) {
        std::lock_guard lock(m_Mutation);
        for(size_t b=0;b<m_Count.load();++b) {const double result=NativeMemoryCheck(m_Api[b].memory,begin,count);if(result!=0) return result;}
        return 0;
    }
    // Called only when the native owner explicitly reinitializes its pool.
    double Reset() {
        std::lock_guard lock(m_Mutation);double result=0;
        if(m_Count.load())result=m_Api[0].init();
        for(size_t b=1;b<m_Count.load();++b) {
            // Native InitPool allocates backing pages; repeated calls do not release
            // those pages. Reuse our existing pools instead of leaking on resets.
            for(size_t i=0;i<Capacity;++i)if(m_Live[b].test(i))m_Api[b].free(double(i));
            m_Live[b].reset();m_Used[b]=0;
        }
        m_OverflowLive.store(0);return result;
    }
    size_t Headroom() {
        std::lock_guard lock(m_Mutation);size_t total=0;
        // Bank zero's pre-installation occupancy is intentionally unknown.
        for(size_t b=1;b<m_Count.load();++b) total+=Capacity-m_Used[b];
        return total;
    }
    bool EnsureHeadroom(size_t desired) {
        std::lock_guard lock(m_Mutation);
        while(Headroom()<desired) if(!AddBank()) return false;
        return true;
    }
    size_t BankCount() const {return m_Count.load();}
    size_t OverflowLive() const {return m_OverflowLive.load();}
    size_t Failures() const {return m_Failures.load();}
    size_t InvalidHandles() const {return m_Invalid.load();}
};
}
