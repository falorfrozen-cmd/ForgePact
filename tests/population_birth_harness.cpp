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
static int reads=0;
static int CallerObjectIndex(CInstance* s){++reads;return s->object;}
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
// PRODUCTION_CALLER_INFO
// PRODUCTION_BIRTH_SCOPE
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    auto& m=ForgePact::MapRevealManager::Instance();
    CInstance caller;RValue args[4]={0,0,0,2};RValue result;
    for(int kind:{VALUE_REAL,VALUE_INT32,VALUE_INT64,VALUE_REF}){
        m={};result={};
        {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);result=RValue(123);result.m_Kind=kind;observation.Completed();}
        check(m.births==1 && m.pending.empty(),"valid native identity");
    }
    for(int bad:{-4,-1}){
        m={};{CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);result=RValue(bad);observation.Completed();}
        check(m.births==0 && m.pending.count(42),"failed native result");
    }
    m={};result=RValue(321);
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);}
    check(m.births==0,"uncalled or throwing original keeps stale result unconfirmed");
    m={};
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);result={};observation.Completed();}
    check(m.births==0,"undefined result");
    m={};
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);++m.generation;result=RValue(222);observation.Completed();}
    check(m.births==0,"reset during native creation");
    m={};caller.object=2;
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);result=RValue(123);observation.Completed();}
    check(m.births==0,"enemy-created child not a pack");
    m={};caller.object=1;args[3]=RValue(3);
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);result=RValue(123);observation.Completed();}
    check(m.births==0,"non-enemy object not a birth");
    m={};args[3]=RValue(2);caller.id=99;
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);result=RValue(123);observation.Completed();}
    check(m.births==0,"unknown caller");
    m={};m.enabled=false;reads=0;
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);result=RValue(123);observation.Completed();}
    check(reads==0 && m.births==0,"disabled fast path");
    m={};caller.id=42;
    {CreationCallerInfo info(&caller);PopulationBirthScope observation(result,&caller,4,args,info);caller.id=-1;result=RValue(123);observation.Completed();}
    check(m.births==1,"caller identity captured before native call");
    std::cout<<"population native birth observation PASS\n";
}
