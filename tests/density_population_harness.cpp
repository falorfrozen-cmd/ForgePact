#define FORGEPACT_RELEASE 1
#define BP_DIAG_INCREMENT(x) ((void)0)
#include <ForgePact/AdaptivePopulationBudget.hpp>
#include <ForgePact/DeferredDensityCopies.hpp>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdint>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include <deque>
enum {VALUE_REAL,VALUE_INT32,VALUE_INT64,VALUE_REF,VALUE_STRING,VALUE_OBJECT,VALUE_UNDEFINED};
struct CInstance;
struct RValue{
    int m_Kind=VALUE_UNDEFINED;double n=0;CInstance* ptr=nullptr;std::string s;
    RValue()=default;RValue(double value):m_Kind(VALUE_REAL),n(value){}
    RValue(const char* value):m_Kind(VALUE_STRING),s(value){}
    explicit RValue(CInstance* value):m_Kind(VALUE_OBJECT),ptr(value){}
    double ToDouble()const{return n;}bool ToBoolean()const{return n!=0;}
};
struct CInstance{int id;RValue ToRValue(){return RValue(this);}};
static CInstance global{-1},parent{10},player{20};
static bool parentAlive=true,mapReady=true,capacity=true,playerPresent=true;
static int64_t room=1;static uint64_t g_RuntimeFrame=0,clockValue=0;
static uint64_t fakeClock(){return clockValue;}
static bool AurieSuccess(int status){return status==0;}
struct FakeYY{
    int GetGlobalInstance(CInstance** out){*out=&global;return 0;}
    RValue CallBuiltin(const char* name,std::initializer_list<RValue> args){
        std::vector<RValue> a(args);
        if(std::string(name)=="variable_instance_get"){
            if(a[1].s=="id")return RValue(static_cast<double>(a[0].ptr->id));
            if(a[1].s=="x" || a[1].s=="y")return RValue(0.0);
        }
        throw std::runtime_error("unexpected builtin");
    }
} api;
static FakeYY* g_Yytk=&api;
static int64_t CurrentRoomKey(){return room;}
static bool HhResolveLocalPlayer(RValue& out){out=player.ToRValue();return playerPresent;}
static CInstance* HhResolveInstance(const RValue& id){return id.ToDouble()==10 && parentAlive?&parent:nullptr;}
static bool PopulationCapacityAvailable(){return capacity;}
namespace ForgePact{
struct DensityManager{double Mult=4,Frac=0;static DensityManager& Instance(){static DensityManager d;return d;}};
struct MapRevealManager{
    bool enabled=true,packs=true,window=true;
    static MapRevealManager& Instance(){static MapRevealManager m;return m;}
    bool HasReadableMap()const{return mapReady;}
    bool IsEnabled()const{return enabled;}bool PacksEnabled()const{return packs;}
    bool WantsPackSpawn()const{return window;}
};
}
using TRoutine=void(*)(RValue&,CInstance*,CInstance*,int,RValue*);
static std::vector<double> createdX;
static std::vector<int> createdSelf;
static void nativeCreate(RValue& result,CInstance* self,CInstance*,int,RValue* args){
    if(self==&parent && !parentAlive)throw std::runtime_error("stale caller");
    createdX.push_back(args[0].ToDouble());createdSelf.push_back(self?self->id:0);
    result=RValue(static_cast<double>(1000+createdX.size()));clockValue+=1200;
}
static TRoutine g_OrigICD=nativeCreate,g_OrigICL=nativeCreate;
static bool g_KuyruktanYaratim=false,g_CallerIsEnemy=false;
static unsigned g_SpecialCreateDepth=0;static int g_EnemyMultAll=1,specialMultiplier=1;
static int g_ExtraCreators=0,g_ExtraEnemies=0,g_DensitySkippedEnemyBorn=0;
static void InterlockedIncrement(int* value){++*value;}
struct SpecialCreateScope{bool active;explicit SpecialCreateScope(bool v):active(v){if(active)++g_SpecialCreateDepth;}~SpecialCreateScope(){if(active)--g_SpecialCreateDepth;}};
static int ObjectMultiplier(int){return specialMultiplier;}
static bool IsCachedCreatorObject(int obj){return obj==41;}
static bool IsCreatorObject(int obj){return obj==41;}
static bool IsEnemyObject(int){return false;}
static bool DensityWindowActive(){return true;}
static void NoteDensityCreator(){}
static int g_KareBasina=3;static uint64_t g_KuyrukKare=0,g_KuyrukToplam=0;
struct GecikmeliYaratim{bool katman;double x,y;RValue yuva,nesne;uint64_t dogum;};
static std::deque<GecikmeliYaratim> g_Kuyruk;
// PRODUCTION_DENSITY_KEYS
// PRODUCTION_DENSITY_RUNTIME
// PRODUCTION_MULTI_CREATE
static void check(bool yes){if(!yes)throw std::runtime_error("density integration invariant");}
static void frame(){clockValue+=16667;ForgePact::AdaptivePopulationBudget::Instance().BeginFrame(++g_RuntimeFrame,true);DensityCopiesTick();}
static void create(double x,int argc=4,CInstance* self=&parent){
    RValue a[5]={RValue(x),RValue(50.0),RValue(0.0),RValue(41.0),RValue(&global)},out;
    DoMultiCreate(nativeCreate,out,self,nullptr,argc,a,41,false);
}
int main(){
    ForgePact::AdaptivePopulationBudget::Instance()=ForgePact::AdaptivePopulationBudget(&fakeClock);
    create(100);check(createdX.size()==1 && DeferredDensityPending()==3);
    mapReady=false;frame();check(createdX.size()==1);
    mapReady=true;capacity=false;frame();check(createdX.size()==1);
    capacity=true;frame();check(createdX.size()>1 && createdX.size()<4);
    for(int i=0;i<30 && DeferredDensityPending();++i)frame();
    check(createdX.size()==4 && DeferredDensityPending()==0);
    for(auto self:createdSelf)check(self==10);
    ResetDeferredDensity(false);create(100);frame();check(createdX.size()==5 && DeferredDensityPending()==0);
    std::cout<<"production: original synchronous, copies paced, readiness/capacity, caller identity, revisit PASS\n";

    create(300);parentAlive=false;auto prior=createdX.size();frame();check(createdX.size()==prior);
    parentAlive=true;for(int i=0;i<90 && DeferredDensityPending();++i)frame();
    check(createdX.size()==prior+3);
    create(500);prior=createdX.size();room=2;frame();check(createdX.size()==prior && DeferredDensityPending()==0);
    create(500);for(int i=0;i<30 && DeferredDensityPending();++i)frame();check(createdX.size()==prior+4);
    std::cout<<"production: missing caller waits, no cross-room spawn, partial plan resumes PASS\n";

    prior=createdX.size();create(700,5);check(createdX.size()==prior+4 && g_DensityCopyRefused==1);
    auto& dm=ForgePact::DensityManager::Instance();dm.Mult=1;
    prior=createdX.size();create(900);check(createdX.size()==prior+1 && DeferredDensityPending()==0);
    dm.Mult=4;g_SpecialCreateDepth=1;prior=createdX.size();create(1100);g_SpecialCreateDepth=0;
    check(createdX.size()==prior+1 && DeferredDensityPending()==0);
    std::cout<<"production: unknown init struct preserves native call, density off, special child exempt PASS\n";
    std::cout<<"RESULT OK\n";
}
