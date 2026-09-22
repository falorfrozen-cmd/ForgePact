#pragma once

// Only the explicit local profile build includes this recorder. No watchdog,
// background thread, command surface, or per-call log is installed. Optional
// named-script timing hooks exist only in this local build.
#ifdef FORGEPACT_POPULATION_PROFILE
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace ForgePact::PopulationProfile {
enum class Metric : unsigned {
    DistanceExtra, DistanceNative, PoolGet, PoolSet, PoolAllocate, PoolFree, PoolMemoryCheck,
    FrameCallback, DensityTick, MinerTick, MapTick, NativeCreate,
    HuntScan, HuntLeash, HuntWakeObject, HuntWake, ActivateProps, HuntStartPath,
    EnemyStep, EnemyBeginStep, EnemyGlobalTimers, EnemyEffectTimers,
    PathStep, PathController, PathSeparate, PathRepath, PathBroadcast,
    MinimapDraw, MinimapDynamicDraw, EnemyPackDraw, EnemyHealthDraw,
    PresetData, ZoneKeyPresets, ZoneHeatMap, ZonePresetObjects, ZoneGroundPresets,
    ZoneWalls, ZoneAutotile, ZoneStaticBlockers, ZoneStateLoad, Count
};
inline constexpr const char* Names[]={
    "distance_extra", "distance_native", "protected_get_total", "protected_set_total",
    "protected_allocate_total", "protected_free_total", "protected_memory_check_total",
    "frame_callback", "density_tick", "miner_tick", "map_tick", "native_create",
    "hunt_scan_total", "hunt_leash_total", "hunt_wake_object", "hunt_wake_total", "activation_total", "hunt_start_path_total",
    "enemy_step_native", "enemy_begin_step_native", "enemy_global_timers_native", "enemy_effect_timers_native",
    "path_step_native", "path_controller_native", "path_separate_native", "path_repath_native", "path_broadcast_native",
    "minimap_draw_native", "minimap_dynamic_draw_native", "enemy_pack_draw_native", "enemy_health_draw_native",
    "preset_data_native", "zone_key_presets_native", "zone_heatmap_native", "zone_preset_objects_native",
    "zone_ground_presets_native", "zone_walls_native", "zone_autotile_native", "zone_static_blockers_native", "zone_state_load_native"
};
static_assert(sizeof(Names)/sizeof(Names[0])==unsigned(Metric::Count));
enum class StartPoint { ReadyCreators, MapGeneration };
struct Reading {uint64_t calls=0,samples=0,sampledUs=0,period=1;};
class Recorder {
public:
    using Clock=uint64_t(*)();
    static uint64_t Micros(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    explicit Recorder(Clock clock=&Micros):m_Clock(clock){}
    static constexpr unsigned Period(Metric m){
        return m==Metric::PoolGet || m==Metric::PoolSet?64:
            m==Metric::DistanceExtra || m==Metric::DistanceNative ||
            m==Metric::HuntScan || m==Metric::HuntLeash || m==Metric::HuntStartPath ||
            m==Metric::EnemyBeginStep || m==Metric::EnemyGlobalTimers || m==Metric::EnemyEffectTimers ||
            m==Metric::PathStep || m==Metric::PathSeparate || m==Metric::PathRepath ||
            m==Metric::PathBroadcast || m==Metric::EnemyHealthDraw?16:1;
    }
    void Begin(uint64_t durationUs=90000000,StartPoint point=StartPoint::ReadyCreators){
        if(m_Active.load(std::memory_order_relaxed) || m_Finished)return;
        m_Start=m_Clock();m_Duration=durationUs;m_LastPresent=m_Start;m_StartPoint=point;
        m_Active.store(true,std::memory_order_release);
    }
    bool Active()const{return m_Active.load(std::memory_order_relaxed);}
    bool Finished()const{return m_Finished;}
    const char* StartedAt()const{return m_StartPoint==StartPoint::MapGeneration?"map_generation":"ready_creators";}
    uint64_t Now()const{return m_Clock();}
    // Count all intercepted calls, time a deterministic fraction. Atomic counts
    // tolerate native extension calls from another thread; snapshots are approximate.
    bool Sample(Metric m){
        if(!Active())return false;
        return m_Data[unsigned(m)].calls.fetch_add(1,std::memory_order_relaxed)%Period(m)==0;
    }
    void FinishSample(Metric m,uint64_t elapsed){
        if(!Active())return;
        auto& d=m_Data[unsigned(m)];d.sampledUs.fetch_add(elapsed,std::memory_order_relaxed);
        d.samples.fetch_add(1,std::memory_order_relaxed);
    }
    void RecordNative(uint64_t elapsed){
        if(!Active())return;
        m_Data[unsigned(Metric::NativeCreate)].calls.fetch_add(1,std::memory_order_relaxed);
        FinishSample(Metric::NativeCreate,elapsed);
    }
    // Only the game's existing frame callback calls these lifecycle functions.
    void Frame(){
        if(!Active())return;
        const auto now=m_Clock();m_LastFrame=now-m_LastPresent;m_LastPresent=now;++m_Frames;
        if(m_LastFrame>m_PeakFrame)m_PeakFrame=m_LastFrame;
    }
    bool Expire(){
        if(!Active())return false;
        const auto elapsed=m_Clock()-m_Start;if(elapsed<m_Duration)return false;
        m_Active.store(false,std::memory_order_release);m_Elapsed=elapsed;m_Finished=true;return true;
    }
    uint64_t ElapsedUs()const{return Active()?m_Clock()-m_Start:m_Elapsed;}
    uint64_t FrameCount()const{return m_Frames;}
    uint64_t LastFrameUs()const{return m_LastFrame;}
    uint64_t PeakFrameUs()const{return m_PeakFrame;}
    Reading Read(Metric m)const{
        const auto& d=m_Data[unsigned(m)];
        return {d.calls.load(std::memory_order_relaxed),d.samples.load(std::memory_order_relaxed),
            d.sampledUs.load(std::memory_order_relaxed),Period(m)};
    }
private:
    struct Counter {std::atomic<uint64_t> calls{0},samples{0},sampledUs{0};};
    std::array<Counter,unsigned(Metric::Count)> m_Data{};
    Clock m_Clock;
    std::atomic<bool> m_Active{false};
    bool m_Finished=false;
    StartPoint m_StartPoint=StartPoint::ReadyCreators;
    uint64_t m_Start=0,m_Duration=0,m_Elapsed=0,m_Frames=0,m_LastPresent=0,m_LastFrame=0,m_PeakFrame=0;
};
inline Recorder& Instance(){static Recorder recorder;return recorder;}
class Scope {
    Recorder& m_Recorder;
    Metric m_Metric;
    bool m_Sampled;
    uint64_t m_Start;
public:
    Scope(Recorder& r,Metric m):m_Recorder(r),m_Metric(m),m_Sampled(r.Sample(m)),m_Start(m_Sampled?r.Now():0){}
    ~Scope(){if(m_Sampled && m_Recorder.Active())m_Recorder.FinishSample(m_Metric,m_Recorder.Now()-m_Start);}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
#define FP_POP_JOIN_IMPL(a,b) a##b
#define FP_POP_JOIN(a,b) FP_POP_JOIN_IMPL(a,b)
#define FP_POP_SCOPE(metric) ::ForgePact::PopulationProfile::Scope FP_POP_JOIN(fpPopulationScope_,__LINE__)(::ForgePact::PopulationProfile::Instance(),::ForgePact::PopulationProfile::Metric::metric)
#define FP_POP_BEGIN() ::ForgePact::PopulationProfile::Instance().Begin()
#define FP_POP_NATIVE(elapsed) ::ForgePact::PopulationProfile::Instance().RecordNative(elapsed)
#else
#define FP_POP_SCOPE(...) ((void)0)
#define FP_POP_BEGIN() ((void)0)
#define FP_POP_NATIVE(...) ((void)0)
#endif
