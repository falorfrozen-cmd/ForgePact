#include <ForgePact/PopulationProfile.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#undef assert
#define assert(c) do{if(!(c)){std::cerr<<"FAILED " #c "\n";return 1;}}while(false)

#ifdef FORGEPACT_POPULATION_PROFILE
struct CInstance {};
struct RValue { int value=0; };
using PFUNC_YYGMLScript=RValue&(*)(CInstance*,CInstance*,RValue&,int,RValue**);
using PVOID=void*;
static CInstance self,other;
static RValue alternate{45},result,argsValue{9};
static RValue* args[]={&argsValue};
static int calls=0;
static bool forwarded=true;
static RValue& Native(CInstance* s,CInstance* o,RValue& r,int n,RValue** a){
    ++calls;forwarded=forwarded && s==&self && o==&other && &r==&result && n==1 && a==args;
    r.value+=a[0]->value;return alternate;
}
static std::vector<PFUNC_YYGMLScript> wrappers;
static bool HookOneScript(const char* name,const char*,PVOID dest,PFUNC_YYGMLScript* orig,bool* native){
    // Exercise complete failure AND a table-only attachment. Both must be
    // visible in coverage instead of appearing as native zero-cost evidence.
    if(std::string(name)=="DrawEnemyPack")return false;
    *orig=Native;*native=std::string(name)!="EnemyChildStepEffectTimers";
    wrappers.push_back(reinterpret_cast<PFUNC_YYGMLScript>(dest));return true;
}
#endif
#include <ForgePact/PopulationScriptProfile.hpp>
int main(){
#ifdef FORGEPACT_POPULATION_PROFILE
    using namespace ForgePact::PopulationProfile;
    InstallScriptTimings();assert(wrappers.size()==21);
    InstallScriptTimings();assert(wrappers.size()==21);
    assert(!Instance().Active());
    // A map's first native preset-data call starts capture before generation,
    // rather than waiting until ready enemy creators exist after the load.
    auto& initial=Hook_PopulatePresetData(&self,&other,result,1,args);
    assert(&initial==&alternate && Instance().Active());
    assert(std::string(Instance().StartedAt())=="map_generation");
    Instance().Begin(); // later ready-creator notification cannot reset origin
    assert(std::string(Instance().StartedAt())=="map_generation");
    for(auto hook:wrappers){auto& r=hook(&self,&other,result,1,args);assert(&r==&alternate);}
    assert(forwarded && calls==22 && result.value==22*9);
    assert(Instance().Read(Metric::EnemyStep).calls==1);
    for(auto m:{Metric::PresetData,Metric::ZoneKeyPresets,Metric::ZoneHeatMap,Metric::ZonePresetObjects,
        Metric::ZoneGroundPresets,Metric::ZoneWalls,Metric::ZoneAutotile,Metric::ZoneStaticBlockers,Metric::ZoneStateLoad})
        assert(Instance().Read(m).calls==(m==Metric::PresetData?2:1));
    std::ostringstream coverage;WriteScriptCoverage(coverage);
    assert(coverage.str().find("\"DrawEnemyPack\":{\"table\":false,\"native\":false}")!=std::string::npos);
    assert(coverage.str().find("\"EnemyChildStepEffectTimers\":{\"table\":true,\"native\":false}")!=std::string::npos);
    std::cout<<"script timing forwarding PASS\n";
#else
    // Compiles with no game ABI, hook installer, or recorder in a player build.
    std::cout<<"script timing disabled PASS\n";
#endif
}
