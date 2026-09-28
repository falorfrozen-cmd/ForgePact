// Runs the production mining adapter, including clone, scope and dispatch,
// the extra rolls and the shared CombatText detour (CombatTextHook.hpp).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <hs_game_sdk/item_type.hpp>
#include <hs_game_sdk/scripts.hpp>

enum { VALUE_REAL, VALUE_STRING, VALUE_OBJECT, VALUE_ARRAY, VALUE_UNDEFINED,
       VALUE_INT32, VALUE_INT64, VALUE_REF, VALUE_BOOL };
struct Fields;
struct RValue {
    int m_Kind=VALUE_UNDEFINED;
    double number=0;
    std::string text;
    std::shared_ptr<Fields> m_Object;
    RValue()=default;
    RValue(double v):m_Kind(VALUE_REAL),number(v){}
    RValue(const char* s):m_Kind(VALUE_STRING),text(s){}
    double ToDouble() const { if(m_Kind!=VALUE_REAL&&m_Kind!=VALUE_INT32&&m_Kind!=VALUE_INT64&&m_Kind!=VALUE_REF) throw std::runtime_error("not numeric"); return number; }
    bool ToBoolean() const { return number!=0; }
    std::string ToString() const { return text; }
};
// A node is reached the runner's way: its reference, then instance builtins.
struct CInstance {
    double id=0;
    RValue ToRValue() const { RValue v(id); v.m_Kind=VALUE_REF; return v; }
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
static CInstance node{200}, other{300};
// The node's instance variables (hp, miningQue, miningActivateDistance, stop).
static std::map<std::string,RValue> nodeVars;
static int instanceWrites=0;
static bool failInstanceSet=false;
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
        if(n=="variable_instance_get"||n=="variable_instance_set") {
            if(a[0].m_Kind!=VALUE_REF||a[0].number!=node.id) throw std::runtime_error("instance builtin on an unknown instance");
            if(n=="variable_instance_get"){const auto it=nodeVars.find(a[1].text);return it==nodeVars.end()?RValue():it->second;}
            ++instanceWrites;
            if(failInstanceSet) throw std::runtime_error("instance write failed");
            nodeVars[a[1].text]=a[2];return RValue();
        }
        throw std::runtime_error("unexpected builtin "+n);
    }
};
static Runtime runtime;
static Runtime* g_Yytk=&runtime;

// PRODUCTION_MINING_ORE

// The five side-effect scripts the dig calls directly, as the game's own
// functions: each counts its calls; CombatText keeps the text it was given.
static const char* const kScriptNames[5]={"MiningAdd","ExperienceUpdate","GuildExperienceAdd","update_quest","CombatText"};
static int nativeScriptCalls[5]={};
static std::vector<std::string> floatingTexts;
template<int I> RValue& NativeScript(CInstance*,CInstance*,RValue& r,int argc,RValue** a) {
    ++nativeScriptCalls[I];
    if(I==4&&argc>0&&a&&a[0]) floatingTexts.push_back(a[0]->text);
    return r;
}
static PFUNC_YYGMLScript kNativeScripts[5]={NativeScript<0>,NativeScript<1>,NativeScript<2>,NativeScript<3>,NativeScript<4>};
// What the production code installed, per script (the fake script table).
static std::map<std::string,PFUNC_YYGMLScript> detours;
static std::map<std::string,int> installsByName;
static std::set<std::string> tableOnly;
static void CallScript(int i,const char* text=nullptr) {
    RValue r, arg(text?text:"");RValue* args[]={&arg};
    const auto it=detours.find(kScriptNames[i]);
    (it!=detours.end()?it->second:kNativeScripts[i])(&node,&node,r,text?1:0,args);
}

static RValue params;
static double category=14;
static bool executeReward=true, staleAfterFirstRun=false;
static int throwOnStep=0, paidRuns=0, dispatches=0;
static std::vector<double> drops;
static RValue& NativeDrop(CInstance* s,CInstance* o,RValue& r,int argc,RValue** args) {
    ++originalCalls;
    received=args[3]->m_Object;
    observed=received->values.count("o")?received->values["o"].number:1;
    drops.push_back(observed);
    if(recurse) {
        recurse=false;
        RValue nested;
        ForgePact::MiningOre::HookLoot(s,o,nested,argc,args);
        nestedObserved=observed;
    }
    if(throwDrop)throw std::runtime_error("native drop");
    r=RValue(777.0);return r;
}
// The game's dig completion: it pays only while the node's hp is 1, then
// leaves it at 0 and calls the five side-effect scripts. With
// staleAfterFirstRun it models a completion that reads something the first
// run changed, so a re-run pays nothing and leaves hp where it was put.
static RValue& NativeStep(CInstance* s,CInstance* o,RValue& r,int,RValue**) {
    ++stepCalls;
    if(throwStep||stepCalls==throwOnStep)throw std::runtime_error("native step");
    if(!executeReward||nodeVars["hp"].number!=1)return r;
    if(staleAfterFirstRun&&paidRuns>0)return r;
    ++paidRuns;
    nodeVars["hp"]=RValue(0.0);nodeVars["miningQue"]=RValue(0.0);
    RValue x(10.0),y(20.0),type(category),unused;
    RValue* args[]={&x,&y,&type,&params,&unused,&unused};
    ForgePact::MiningOre::HookLoot(s,o,r,6,args);
    for(int i=0;i<4;++i)CallScript(i);
    CallScript(4,"10 XP");
    return r;
}
static PFUNC_YYGMLScript OriginalFor(const std::string& n) {
    if(n=="MiningNodeStepMain")return NativeStep;
    for(int i=0;i<5;++i)if(n==kScriptNames[i])return kNativeScripts[i];
    return NativeDrop;
}
// As ModuleMain's HookOneScript: the table entry becomes the detour; only the
// first install of a name can come up native (a second finds this module's
// hook in the table and is table-only).
static bool HookOneScript(const char* n,const char*,PVOID dest,PFUNC_YYGMLScript* orig,bool* native) {
    ++installed;
    const std::string name(n);
    const int count=++installsByName[name];
    *native=nativeAvailable&&!tableOnly.count(name)&&count==1;
    if(!*orig)*orig=OriginalFor(name);
    detours[name]=reinterpret_cast<PFUNC_YYGMLScript>(dest);
    return true;
}
static void Check(bool condition,const char* label) {
    std::cout<<(condition?"PASS ":"FAIL ")<<label<<"\n"; if(!condition)++failures;
}
static bool Logged(const std::string& text) {
    return std::any_of(logs.begin(),logs.end(),[&](const std::string& l){return l.find(text)!=std::string::npos;});
}
static RValue MakeParams(double base=27,double quantity=2) {
    RValue p;p.m_Kind=VALUE_OBJECT;p.m_Object=std::make_shared<Fields>();
    p.m_Object->values={{"b",RValue(base)},{"o",RValue(quantity)},{"j",RValue(123.0)}};
    return p;
}
// One node finishing: the player's dig brings it to the completion with hp 1.
static void RunStep() {
    nodeVars["hp"]=RValue(1.0);nodeVars["miningQue"]=RValue(0.0);
    RValue r; ForgePact::MiningOre::HookStep(&node,&node,r,0,nullptr);
}
static bool AllDrops(size_t count,double each) {
    return drops.size()==count&&std::all_of(drops.begin(),drops.end(),[&](double d){return d==each;});
}
static bool ScriptsOnce() {
    return std::all_of(std::begin(nativeScriptCalls),std::end(nativeScriptCalls),[](int n){return n==1;});
}
static void Reset() {
    using namespace ForgePact::MiningOre;
    multiplier=1;installTried=false;ready=false;unavailable=false;loggedReward=false;loggedFailure=false;
    activeNode=nullptr;inReward=false;originalStep=NativeStep;originalLoot=NativeDrop;
    rewardMultiplier=nullptr;rewardDispatched=nullptr;installFailure.clear();
    rolls=1;rollsInstallTried=rollsReady=rollsUnavailable=false;rollsFailure.clear();inExtraRun=false;
    loggedRollPaid=loggedRollUnpaid=loggedHpReset=false;extraRuns=extraRunsUnpaid=silencedCalls=0;
    for(auto& s:silencedScripts){s.original=nullptr;s.native=false;s.passed=s.silenced=0;}
    namespace C=ForgePact::CombatText;
    C::original=nullptr;C::installTried=C::installed=C::native=false;C::xpMultiplier=1;C::rewardScopeActive=nullptr;
    C::silenceDepth=0;C::passedCalls=C::silencedCalls=0;
    builtins=originalCalls=stepCalls=cloneCalls=installed=0;
    failClone=aliasClone=failSet=failRead=throwDrop=throwStep=recurse=false;
    nativeAvailable=true;observed=nestedObserved=-1;logs.clear();category=14;executeReward=true;params=MakeParams();
    nodeVars={{"hp",RValue(1.0)},{"miningQue",RValue(0.0)},{"miningActivateDistance",RValue(16.0)},{"stop",RValue(0.0)}};
    instanceWrites=0;failInstanceSet=false;staleAfterFirstRun=false;throwOnStep=paidRuns=dispatches=0;drops.clear();
    std::fill(std::begin(nativeScriptCalls),std::end(nativeScriptCalls),0);floatingTexts.clear();
    detours.clear();installsByName.clear();tableOnly.clear();
}
int main() {
    using namespace ForgePact::MiningOre;
    // ===== Extra rolls: the vanilla baseline first =====
    Reset();RollsCommand("1");RunStep();
    Check(stepCalls==1&&originalCalls==1&&observed==2&&installed==0&&builtins==0&&nodeVars["hp"].number==0
        &&ScriptsOnce()&&floatingTexts.size()==1&&floatingTexts[0]=="10 XP","baseline/rolls1_single_step_no_extra_hooks");
    Check(rolls==1&&Logged("miningrolls: x1 (vanilla)"),"target/rolls1_prints_vanilla");
    // Three rolls at multiplier 1 with no helmet: three sets at the original quantity.
    Reset();RollsCommand("3");RunStep();
    Check(stepCalls==3&&AllDrops(3,2)&&nodeVars["hp"].number==0&&extraRuns==2&&extraRunsUnpaid==0&&multiplier==1
        &&rollsReady&&!rollsUnavailable&&ready&&!unavailable&&params.m_Object->values["o"].number==2
        &&Logged("miningrolls: x3 (each dig rolled 3 times)")&&Logged("miningrolls: first extra roll paid 1 ore stacks")
        &&!Logged("node left diggable"),"target/rolls3_three_payouts_hp_ends_0");
    Check(installed==7&&installsByName.size()==7&&installsByName["CombatText"]==1&&installsByName["update_quest"]==1
        &&installsByName["GuildExperienceAdd"]==1,"target/rolls3_installs_seven_native_detours_once");
    Reset();Command("5");RollsCommand("3");RunStep();
    Check(stepCalls==3&&AllDrops(3,10)&&params.m_Object->values["o"].number==2&&nodeVars["hp"].number==0,
        "target/rolls3_with_multiplier_x5_each_stack");
    Reset();RollsCommand("3");rewardMultiplier=[](CInstance*){return 4;};
    rewardDispatched=[](CInstance*,double,double){++dispatches;};RunStep();
    Check(stepCalls==3&&AllDrops(3,8)&&dispatches==1,"target/helmet_x4_each_run_dispatch_once");
    Reset();Command("10");RollsCommand("3");rewardMultiplier=[](CInstance*){return 4;};
    rewardDispatched=[](CInstance*,double,double){++dispatches;};RunStep();
    Check(AllDrops(3,8)&&dispatches==1,"target/helmet_x4_each_run_dispatch_once");
    // A re-run that pays nothing ends the loop and never leaves the node diggable.
    Reset();RollsCommand("3");staleAfterFirstRun=true;RunStep();
    Check(stepCalls==2&&drops.size()==1&&extraRuns==1&&extraRunsUnpaid==1&&nodeVars["hp"].number==0
        &&Logged("miningrolls: extra roll paid nothing - stopped after 0 of 2 extra rolls")
        &&Logged("miningrolls: node left diggable, hp reset to 0")&&ScriptsOnce(),"target/unpaid_rerun_stops_and_restores_hp0");
    Reset();RollsCommand("3");executeReward=false;RunStep();
    Check(stepCalls==1&&extraRuns==0&&originalCalls==0&&instanceWrites==0&&nodeVars["hp"].number==1,"target/no_first_reward_no_extra_run");
    Reset();RollsCommand("3");category=15;RunStep();
    Check(stepCalls==1&&extraRuns==0&&drops.size()==1&&instanceWrites==0,"target/no_first_reward_no_extra_run");
    // XP, guild XP, quests and the floating text: once per dig, silenced and counted inside extra runs.
    Reset();RollsCommand("3");RunStep();
    bool passedOnce=true, silencedTwice=true;
    for(const auto& s:silencedScripts){passedOnce=passedOnce&&s.passed==1;silencedTwice=silencedTwice&&s.silenced==2;}
    Check(ScriptsOnce()&&passedOnce&&silencedTwice&&silencedCalls==8&&ForgePact::CombatText::silencedCalls==2
        &&ForgePact::CombatText::passedCalls==1&&SilencedCalls()==10&&floatingTexts.size()==1,"target/xp_silenced_only_inside_extra_runs");
    CallScript(0);CallScript(4,"7 XP");
    Check(nativeScriptCalls[0]==2&&nativeScriptCalls[4]==2&&SilencedCalls()==10,"target/xp_silenced_only_inside_extra_runs");
    // An exception in an extra run propagates once, is never retried, and hp still ends 0.
    Reset();RollsCommand("3");throwOnStep=2;bool caught=false;try{RunStep();}catch(...){caught=true;}
    Check(caught&&stepCalls==2&&drops.size()==1&&extraRuns==1&&nodeVars["hp"].number==0&&!inExtraRun
        &&ForgePact::CombatText::silenceDepth==0&&activeNode==nullptr&&!inReward,"target/extra_run_exception_no_retry_hp0");
    // A pass-through detour that comes up table-only refuses the rolls, and names it; the multiplier still works.
    Reset();tableOnly.insert("update_quest");RollsCommand("3");
    Check(rolls==1&&rollsUnavailable&&!rollsReady&&ready&&!unavailable&&multiplier==1
        &&Logged("miningrolls: unavailable - update_quest came up table-only"),"target/table_only_silence_hook_refuses_rolls_keeps_multiplier");
    Command("5");RunStep();
    Check(multiplier==5&&stepCalls==1&&AllDrops(1,10)&&extraRuns==0,"target/table_only_silence_hook_refuses_rolls_keeps_multiplier");
    Reset();nativeAvailable=false;RollsCommand("3");
    Check(rolls==1&&rollsUnavailable&&unavailable&&!ready&&multiplier==1
        &&Logged("miningrolls: unavailable - MiningNodeStepMain came up table-only"),"target/table_only_step_names_it");
    // The cap is the plugin's own, whatever the panel sends.
    Reset();for(const char* bad:{"0","11","5garbage","2.5","-1","99999999999999999999","","stat"}){
        logs.clear();RollsCommand(bad);
        Check(rolls==1&&installed==0&&Logged("miningrolls: use a whole-number roll count from 1 to 10"),"target/rolls_cap_10_and_bad_values");
    }
    Reset();RollsCommand("10");RunStep();
    Check(rolls==10&&stepCalls==10&&AllDrops(10,2)&&extraRuns==9&&nodeVars["hp"].number==0&&ScriptsOnce(),"target/rolls_cap_10_and_bad_values");
    Reset();RollsCommand("3");RunStep();RollsCommand("1");builtins=stepCalls=originalCalls=0;drops.clear();RunStep();
    Check(rolls==1&&stepCalls==1&&AllDrops(1,2)&&builtins==0,"baseline/rolls_reset_is_vanilla");
    // One CombatText detour serves both mods, in either install order.
    Reset();ForgePact::CombatText::xpMultiplier=2;ForgePact::CombatText::Install();RollsCommand("3");RunStep();
    Check(rollsReady&&installsByName["CombatText"]==1&&ForgePact::CombatText::native&&floatingTexts.size()==1
        &&floatingTexts[0]=="20 XP"&&ForgePact::CombatText::silencedCalls==2&&AllDrops(3,2),"target/combattext_shared_with_xp_multiplier");
    Reset();RollsCommand("3");ForgePact::CombatText::xpMultiplier=2;const bool xpNative=ForgePact::CombatText::Install();RunStep();
    Check(rollsReady&&xpNative&&installsByName["CombatText"]==1&&floatingTexts.size()==1&&floatingTexts[0]=="20 XP"
        &&ForgePact::CombatText::silencedCalls==2&&AllDrops(3,2),"target/combattext_rolls_first_then_xp");
    Reset();ForgePact::CombatText::rewardScopeActive=[]{return true;};ForgePact::CombatText::xpMultiplier=2;
    ForgePact::CombatText::Install();RunStep();
    Check(floatingTexts.size()==1&&floatingTexts[0]=="10 XP","baseline/combattext_reward_scope_leaves_text");

    // ===== The ore multiplier, as before =====
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
    Reset();Command("5");throwDrop=true;caught=false;try{RunStep();}catch(...){caught=true;}
    Check(caught&&originalCalls==1&&activeNode==nullptr&&!inReward,"target/native_exception_no_retry_scope_restored");
    Reset();Command("5");throwStep=true;caught=false;try{RunStep();}catch(...){caught=true;}
    Check(caught&&stepCalls==1&&activeNode==nullptr,"target/step_exception_scope_restored");
    Reset();Command("5");recurse=true;RunStep();Check(nestedObserved==10,"target/nested_reward_not_scaled_twice");
    Reset();Command("5");RollsCommand("3");recurse=true;RunStep();Check(nestedObserved==10&&stepCalls==3,"target/nested_reward_not_scaled_twice");
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
