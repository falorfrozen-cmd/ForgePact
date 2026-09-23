#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
enum {VALUE_REAL,VALUE_INT32,VALUE_INT64,VALUE_REF,VALUE_OBJECT,VALUE_UNDEFINED};
struct CInstance;
struct RValue {
    int m_Kind=VALUE_UNDEFINED;double value=0;CInstance* ptr=nullptr;
    RValue()=default;RValue(double n):m_Kind(VALUE_REAL),value(n){}
    double ToDouble()const {return value;}
};
struct CInstance {int object=1,id=42;RValue ToRValue(){RValue r;r.ptr=this;return r;}};
static int reads=0,objectReads=0;
static int CallerObjectIndex(CInstance* s){++reads;++objectReads;return s?s->object:-1;}
static bool IsEnemyObject(int i){return i==2;}
static bool IsCachedCreatorObject(int i){return i==1;}
static double InstanceIdOf(const RValue& r){++reads;return r.ptr->id;}
namespace ForgePact {
struct MapRevealManager {
    bool enabled=true;uint64_t generation=1,births=0;
    std::unordered_set<int64_t> pending{42};
    static MapRevealManager& Instance(){static MapRevealManager m;return m;}
    bool NeedsBirthObservation()const{return enabled && !pending.empty();}
    bool TracksCreator(int64_t id)const{return pending.count(id)!=0;}
    uint64_t PopulationGeneration()const{return generation;}
    void ObserveNativeBirth(int64_t id){births+=pending.erase(id);}
};
}
static bool rarity=true,tyrant=true,g_CreatingFromEnemy=false,g_CallerIsEnemy=false;
static std::unordered_set<int> g_EnemyBornIds;
static int g_EnemyBornSeen=0;
static void InterlockedIncrement(int* n){++*n;}
static bool RarityFloorActive(){return rarity;}
static bool TyrantActive(){return tyrant;}
static bool CallerIsEnemyInstance(CInstance* s){return IsEnemyObject(CallerObjectIndex(s));}
namespace ForgePact {struct DensityManager {double Mult=4;static DensityManager& Instance(){static DensityManager d;return d;}};}
// PRODUCTION_CALLER_INFO
// PRODUCTION_BIRTH_SCOPE
// PRODUCTION_ENEMY_SCOPE
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void invoke(CInstance* caller,RValue& result,RValue* args,bool recurse=false){
    // MAKE_SCOPES
    if(recurse){
        check(g_CreatingFromEnemy && g_CallerIsEnemy,"enemy chain active during native construction");
        CInstance child{1,43};RValue inner;
        invoke(&child,inner,args);
        check(g_CreatingFromEnemy && g_CallerIsEnemy,"nested creator preserves outer flags");
    }
    result=RValue(123);
    observation.Completed();
}
int main(){
    auto& m=ForgePact::MapRevealManager::Instance();
    CInstance creator{1,42},enemy{2,44};RValue args[4]={0,0,0,2},result;
    objectReads=reads=0;
    for(int i=0;i<1000;++i){m={};invoke(&creator,result,args);check(m.births==1,"birth observed");}
    std::cout<<"caller object reads per 1000 births: "<<objectReads<<"\n";
    // EXPECT_READ_COUNT
    check(!g_CreatingFromEnemy && !g_CallerIsEnemy && g_EnemyBornIds.empty(),"normal packs are not enemy-born");
    m={};invoke(&enemy,result,args,true);
    check(!g_CreatingFromEnemy && !g_CallerIsEnemy && g_EnemyBornIds.count(123),"nested chain restoration");
    check(m.births==0,"enemy caller not reported as pack");
    m={};objectReads=reads=0;args[3]=RValue(8);invoke(&creator,result,args);
    check(objectReads==0 && m.births==0,"decoration fast path");
    m={};m.enabled=false;rarity=tyrant=false;ForgePact::DensityManager::Instance().Mult=1;
    args[3]=RValue(2);objectReads=reads=0;invoke(&creator,result,args);
    check(reads==0,"all off performs no caller lookup");
    std::cout<<"creation caller forwarding PASS\n";
}
