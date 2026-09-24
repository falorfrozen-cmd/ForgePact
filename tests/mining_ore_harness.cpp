// Runs the production mining adapter, including clone, scope and dispatch.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <hs_game_sdk/item_type.hpp>
#include <hs_game_sdk/scripts.hpp>

enum { VALUE_REAL, VALUE_STRING, VALUE_OBJECT, VALUE_ARRAY, VALUE_UNDEFINED,
       VALUE_INT32, VALUE_INT64, VALUE_REF, VALUE_BOOL };
struct CInstance {};
struct Fields;
struct RValue {
    int m_Kind=VALUE_UNDEFINED;
    double number=0;
    std::string text;
    std::shared_ptr<Fields> m_Object;
    RValue()=default;
    RValue(double v):m_Kind(VALUE_REAL),number(v){}
    RValue(const char* s):m_Kind(VALUE_STRING),text(s){}
    double ToDouble() const { if(m_Kind!=VALUE_REAL&&m_Kind!=VALUE_INT32&&m_Kind!=VALUE_INT64) throw std::runtime_error("not numeric"); return number; }
    bool ToBoolean() const { return number!=0; }
};
struct Fields { std::map<std::string,RValue> values; };
using PFUNC_YYGMLScript=RValue&(*)(CInstance*,CInstance*,RValue&,int,RValue**);
using PVOID=void*;
static int builtins=0, originalCalls=0, stepCalls=0, cloneCalls=0, failures=0, installed=0;
static bool failClone=false, aliasClone=false, failSet=false, failRead=false, throwDrop=false, throwStep=false;
static bool nativeAvailable=true, recurse=false;
static double observed=-1, nestedObserved=-1;
static std::shared_ptr<Fields> received;
static std::vector<std::string> logs;
static CInstance node, other;
static void Out(const std::string& s){ logs.push_back(s); }
static consteval const char* SdkShortScriptName(std::string_view n){return n.data()+11;}
static bool HookOneScript(const char*,const char*,PVOID,PFUNC_YYGMLScript*,bool*);
struct Runtime {
    RValue CallBuiltin(const char* name,std::initializer_list<RValue> args) {
        ++builtins;
        std::vector<RValue> a(args);
        const std::string n(name);
        if(n=="variable_clone") {
            ++cloneCalls;
            if(failClone) throw std::runtime_error("clone unavailable");
            if(aliasClone)return a[0];
            RValue result=a[0];result.m_Object=std::make_shared<Fields>(*a[0].m_Object);return result;
        }
        if(n=="variable_struct_exists")return RValue(double(a[0].m_Object->values.count(a[1].text)>0));
        if(n=="variable_struct_get") {
            if(failRead)throw std::runtime_error("bad read");
            const auto it=a[0].m_Object->values.find(a[1].text);
            return it==a[0].m_Object->values.end()?RValue():it->second;
        }
        if(n=="variable_struct_set") {
            if(!failSet)a[0].m_Object->values[a[1].text]=a[2];return RValue();
        }
        throw std::runtime_error("unexpected builtin "+n);
    }
};
static Runtime runtime;
static Runtime* g_Yytk=&runtime;

// PRODUCTION_MINING_ORE

static RValue params;
static double category=14;
static bool executeReward=true;
static RValue& NativeDrop(CInstance* s,CInstance* o,RValue& r,int argc,RValue** args) {
    ++originalCalls;
    received=args[3]->m_Object;
    observed=received->values.count("o")?received->values["o"].number:1;
    if(recurse) {
        recurse=false;
        RValue nested;
        ForgePact::MiningOre::HookLoot(s,o,nested,argc,args);
        nestedObserved=observed;
    }
    if(throwDrop)throw std::runtime_error("native drop");
    r=RValue(777.0);return r;
}
static RValue& NativeStep(CInstance* s,CInstance* o,RValue& r,int,RValue**) {
    ++stepCalls;
    if(throwStep)throw std::runtime_error("native step");
    if(executeReward) {
        RValue x(10.0),y(20.0),type(category),unused;
        RValue* args[]={&x,&y,&type,&params,&unused,&unused};
        ForgePact::MiningOre::HookLoot(s,o,r,6,args);
    }
    return r;
}
static bool HookOneScript(const char* n,const char*,PVOID,PFUNC_YYGMLScript* orig,bool* native) {
    ++installed;*native=nativeAvailable;
    *orig=std::string(n)=="MiningNodeStepMain"?NativeStep:NativeDrop;return true;
}
static void Check(bool condition,const char* label) {
    std::cout<<(condition?"PASS ":"FAIL ")<<label<<"\n"; if(!condition)++failures;
}
static RValue MakeParams(double base=27,double quantity=2) {
    RValue p;p.m_Kind=VALUE_OBJECT;p.m_Object=std::make_shared<Fields>();
    p.m_Object->values={{"b",RValue(base)},{"o",RValue(quantity)},{"j",RValue(123.0)}};
    return p;
}
static void RunStep() {RValue r; ForgePact::MiningOre::HookStep(&node,&node,r,0,nullptr);}
static void Reset() {
    using namespace ForgePact::MiningOre;
    multiplier=1;installTried=false;ready=false;unavailable=false;loggedReward=false;loggedFailure=false;
    activeNode=nullptr;inReward=false;originalStep=NativeStep;originalLoot=NativeDrop;
    rewardMultiplier=nullptr;rewardDispatched=nullptr;
    builtins=originalCalls=stepCalls=cloneCalls=installed=0;
    failClone=aliasClone=failSet=failRead=throwDrop=throwStep=recurse=false;
    nativeAvailable=true;observed=nestedObserved=-1;logs.clear();category=14;executeReward=true;params=MakeParams();
}
int main() {
    using namespace ForgePact::MiningOre;
    Reset();Command("1");RunStep();
    Check(observed==2&&originalCalls==1&&stepCalls==1&&builtins==0&&installed==0,"baseline/x1_no_reads_no_install_one_call");
    Reset();Command("5");RunStep();
    Check(observed==10&&originalCalls==1&&stepCalls==1&&params.m_Object->values["o"].number==2&&received!=params.m_Object,"target/x5_clone_exactly_one_reward");
    Check(received->values["b"].number==27&&received->values["j"].number==123,"target/type_and_seed_preserved");
    for(int base=27;base<=32;++base){Reset();Command("10");params=MakeParams(base,3);RunStep();Check(observed==30,"target/all_six_ores");}
    Reset();Command("5");params.m_Object->values.erase("o");RunStep();Check(observed==5&&!params.m_Object->values.count("o"),"target/implicit_single_ore");
    Reset();Command("5");category=15;RunStep();Check(observed==2&&cloneCalls==0,"baseline/gems_unmodified");
    Reset();Command("5");params=MakeParams(26);RunStep();Check(observed==2&&cloneCalls==0,"baseline/other_materials_unmodified");
    Reset();Command("5");RValue x(1.0),type(14.0),r;RValue* a[]={&x,&x,&type,&params};
    HookLoot(&node,&node,r,4,a);Check(observed==2&&builtins==0,"baseline/ore_outside_mining_unchanged");
    Reset();Command("5");activeNode=&other;HookLoot(&node,&node,r,4,a);Check(observed==2&&builtins==0,"baseline/unrelated_self_unchanged");activeNode=nullptr;
    for(double invalid:std::initializer_list<double>{0.0,-1.0,1.5,1000000000.0,NAN,INFINITY}){Reset();Command("5");params=MakeParams(27,invalid);RunStep();Check(cloneCalls==0&&originalCalls==1,"target/invalid_quantity_passes_through");}
    Reset();Command("5");failClone=true;RunStep();Check(observed==2&&originalCalls==1&&params.m_Object->values["o"].number==2,"target/clone_error_vanilla_once");
    Reset();Command("5");aliasClone=true;RunStep();Check(observed==2&&params.m_Object->values["o"].number==2,"target/alias_clone_refused");
    Reset();Command("5");failSet=true;RunStep();Check(observed==2&&originalCalls==1,"target/write_failure_vanilla_once");
    Reset();Command("5");failRead=true;RunStep();Check(observed==2&&originalCalls==1,"target/read_failure_vanilla_once");
    Reset();Command("5");throwDrop=true;bool caught=false;try{RunStep();}catch(...){caught=true;}
    Check(caught&&originalCalls==1&&activeNode==nullptr&&!inReward,"target/native_exception_no_retry_scope_restored");
    Reset();Command("5");throwStep=true;caught=false;try{RunStep();}catch(...){caught=true;}
    Check(caught&&stepCalls==1&&activeNode==nullptr,"target/step_exception_scope_restored");
    Reset();Command("5");recurse=true;RunStep();Check(nestedObserved==10,"target/nested_reward_not_scaled_twice");
    Reset();Command("5");RunStep();Command("1");builtins=0;RunStep();Check(observed==2&&builtins==0,"target/reset_is_vanilla");
    Reset();nativeAvailable=false;Command("5");RunStep();Check(multiplier==1&&unavailable&&observed==2,"target/table_only_refused");
    Reset();for(const char* bad:{"0","11","5garbage","2.5","-1","99999999999999999999"}){Command(bad);Check(multiplier==1&&installed==0,"target/invalid_command_no_hook");}
    Reset();Command("5");executeReward=false;RunStep();Check(originalCalls==0&&builtins==0,"baseline/no_reward_no_work");
    Reset();Command("10");rewardMultiplier=[](CInstance*){return 4;};RunStep();
    Check(observed==8&&originalCalls==1,"helmet/4x replaces slider 10x");
    rewardMultiplier=[](CInstance*){return 1;};cloneCalls=0;RunStep();
    Check(observed==2&&cloneCalls==0,"helmet/removal read at next reward immediately");
    Reset();Command("10");rewardMultiplier=[](CInstance*)->int{throw std::runtime_error("unreadable gear");};RunStep();
    Check(observed==2&&originalCalls==1,"helmet/unreadable gear vanilla once");
    Reset();Install();rewardMultiplier=[](CInstance*){return 4;};RunStep();
    Check(observed==8&&multiplier==1,"helmet/works with slider at vanilla");
    Reset();Command("10");rewardMultiplier=[](CInstance*){return 1;};throwDrop=true;caught=false;
    try{RunStep();}catch(...){caught=true;}
    Check(caught&&originalCalls==1,"helmet/unequipped native failure never retries");
    Reset();Command("10");rewardMultiplier=[](CInstance*){return 4;};
    rewardDispatched=[](CInstance*,double,double){throw std::runtime_error("effect failure");};RunStep();
    Check(observed==8&&originalCalls==1,"helmet/effect failure never duplicates reward");
    std::cout<<(failures?"RESULT FAILED":"RESULT OK")<<"\n";return failures?1:0;
}
