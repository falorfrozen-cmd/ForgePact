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
#include <hs_game_sdk/objects.hpp>
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
// The game's other live instances: what native CombatText creates (a
// Combat_Text_obj) and anything else a scenario makes it return. The fake
// runtime's index for Combat_Text_obj is deliberately not the SDK's 973, so
// only a by-name resolution (asset_get_index) finds it.
struct FakeInstance { double objectIndex; std::string text; std::map<std::string,RValue> vars; };
static std::map<double,FakeInstance> liveInstances;
static constexpr double kTextObject=5973, kOtherObject=4001;
static double nextInstanceId=1000;
static int textDestroys=0, otherDestroys=0;
static bool textObjectUnknown=false;
static bool IsLive(const RValue& v) {
    return (v.m_Kind==VALUE_REF||v.m_Kind==VALUE_REAL)&&(liveInstances.count(v.number)>0||v.number==node.id);
}
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
        if(n=="asset_get_index")return RValue(!textObjectUnknown&&a[0].text=="Combat_Text_obj"?kTextObject:-1.0);
        // Instance builtins on anything but a number or a reference are the runner's error, a C++ throw.
        if(n=="instance_exists") {
            if(a[0].m_Kind!=VALUE_REF&&a[0].m_Kind!=VALUE_REAL) throw std::runtime_error("instance_exists on a non-instance");
            return RValue(double(IsLive(a[0])));
        }
        if(n=="instance_destroy") {
            if(a[0].m_Kind!=VALUE_REF&&a[0].m_Kind!=VALUE_REAL) throw std::runtime_error("instance_destroy on a non-instance");
            const auto it=liveInstances.find(a[0].number);
            if(it!=liveInstances.end()){++(it->second.objectIndex==kTextObject?textDestroys:otherDestroys);liveInstances.erase(it);}
            return RValue();
        }
        if((n=="variable_instance_get"||n=="variable_instance_set")&&a[0].m_Kind==VALUE_REF&&liveInstances.count(a[0].number)) {
            FakeInstance& inst=liveInstances[a[0].number];
            if(n=="variable_instance_set"){inst.vars[a[1].text]=a[2];return RValue();}
            if(a[1].text=="object_index")return RValue(inst.objectIndex);
            const auto it=inst.vars.find(a[1].text);return it==inst.vars.end()?RValue():it->second;
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
// functions: each counts its calls. CombatText keeps the text it was given,
// creates a text instance and returns it, as the game's does (static reading,
// docs/mining-ore-research.md "Extra rolls"); with otherObjectAfterStep it
// creates an instance of another object once the step has run that often.
static const char* const kScriptNames[5]={"MiningAdd","ExperienceUpdate","GuildExperienceAdd","update_quest","CombatText"};
static int nativeScriptCalls[5]={};
static std::vector<std::string> floatingTexts;
static int otherObjectAfterStep=0;
static size_t maxLiveTexts=0;
static std::vector<std::string> LiveTexts() {
    std::vector<std::string> texts;
    for(const auto& [id,inst]:liveInstances) if(inst.objectIndex==kTextObject) texts.push_back(inst.text);
    return texts;
}
template<int I> RValue& NativeScript(CInstance*,CInstance*,RValue& r,int argc,RValue** a) {
    ++nativeScriptCalls[I];
    if(I==4) {
        const std::string text=argc>0&&a&&a[0]?a[0]->text:std::string();
        floatingTexts.push_back(text);
        const double id=nextInstanceId++;
        const bool other=otherObjectAfterStep>0&&stepCalls>otherObjectAfterStep;
        liveInstances[id]={other?kOtherObject:kTextObject,text,{}};
        maxLiveTexts=std::max(maxLiveTexts,LiveTexts().size());
        r=RValue(id);r.m_Kind=VALUE_REF;
    }
    return r;
}
static PFUNC_YYGMLScript kNativeScripts[5]={NativeScript<0>,NativeScript<1>,NativeScript<2>,NativeScript<3>,NativeScript<4>};
// The tracker's XP count (ForgePact #185), ExperienceUpdate's second consumer:
// what it was handed, and whether each count came after the game's own call.
static int trackerCaptures=0, trackerCounts=0, xpCallsAtCapture=0;
static bool trackerCountedAfterOriginal=true;
static ForgePact::MiningOre::ExperienceCapture TrackerCapture(int,RValue**) {
    ++trackerCaptures;xpCallsAtCapture=nativeScriptCalls[1];
    return {true,true,10};
}
static void TrackerCount(const ForgePact::MiningOre::ExperienceCapture& c,const RValue&) {
    ++trackerCounts;
    if(!c.counted||nativeScriptCalls[1]!=xpCallsAtCapture+1) trackerCountedAfterOriginal=false;
}
static void TrackerAttach() {
    ForgePact::MiningOre::experienceCapture=TrackerCapture;ForgePact::MiningOre::experienceCount=TrackerCount;
}
// What the production code installed, per script (the fake script table).
static std::map<std::string,PFUNC_YYGMLScript> detours;
static std::map<std::string,int> installsByName;
static std::set<std::string> tableOnly;
static RValue CallScript(int i,const char* text=nullptr) {
    RValue r, arg(text?text:"");RValue* args[]={&arg};
    const auto it=detours.find(kScriptNames[i]);
    return (it!=detours.end()?it->second:kNativeScripts[i])(&node,&node,r,text?1:0,args);
}
// The dig sets fields on the text CombatText returned (font, vspeed, hspeed at
// two of its sites). Anything but a live instance there is the runner's error,
// which it raises as a C++ throw: the 2026-10-19 crash (ForgePact #222).
static int textFieldSets=0;
static void UseText(const RValue& text) {
    if(text.m_Kind!=VALUE_REF||!liveInstances.count(text.number)) throw std::runtime_error("CombatText returned no instance");
    liveInstances[text.number].vars["font"]=RValue(1.0);++textFieldSets;
}

static RValue params;
static double category=14;
static bool executeReward=true, staleAfterFirstRun=false;
static int throwOnStep=0, throwAfterTextOnStep=0, paidRuns=0, dispatches=0;
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
// leaves it at 0, calls the five side-effect scripts and sets a field on the
// text CombatText returned. With staleAfterFirstRun it models a completion
// that reads something the first run changed, so a re-run pays nothing and
// leaves hp where it was put; throwAfterTextOnStep throws once that step
// call has made and used its text.
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
    UseText(CallScript(4,"10 XP"));
    if(stepCalls==throwAfterTextOnStep)throw std::runtime_error("native step after its text");
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
static long CountLogged(const std::string& start) {
    return std::count_if(logs.begin(),logs.end(),[&](const std::string& l){return l.rfind(start,0)==0;});
}
static RValue MakeParams(double base=27,double quantity=2) {
    RValue p;p.m_Kind=VALUE_OBJECT;p.m_Object=std::make_shared<Fields>();
    p.m_Object->values={{"b",RValue(base)},{"o",RValue(quantity)},{"j",RValue(123.0)}};
    return p;
}
// One node finishing: the player's dig brings it to the completion with hp 1.
// `escaped` records whether an exception came back out of the detour, where
// the game would receive it.
static bool escaped=false;
static void RunStep() {
    nodeVars["hp"]=RValue(1.0);nodeVars["miningQue"]=RValue(0.0);escaped=false;
    RValue r; try{ForgePact::MiningOre::HookStep(&node,&node,r,0,nullptr);}catch(...){escaped=true;}
}
static bool AllDrops(size_t count,double each) {
    return drops.size()==count&&std::all_of(drops.begin(),drops.end(),[&](double d){return d==each;});
}
// The four silenced side effects once, and one floating text left on screen.
static bool ScriptsOnce() {
    return std::all_of(nativeScriptCalls,nativeScriptCalls+4,[](int n){return n==1;})&&LiveTexts().size()==1;
}
static const char* FakeExceptionType(){return "class std::runtime_error";}
static void Reset() {
    using namespace ForgePact::MiningOre;
    multiplier=1;installTried=false;ready=false;unavailable=false;loggedReward=false;loggedFailure=false;
    activeNode=nullptr;inReward=false;originalStep=NativeStep;originalLoot=NativeDrop;
    rewardMultiplier=nullptr;rewardDispatched=nullptr;installFailure.clear();
    rolls=1;rollsInstallTried=rollsReady=rollsUnavailable=false;rollsFailure.clear();inExtraRun=false;
    loggedRollPaid=loggedRollUnpaid=loggedHpReset=loggedRollFailed=false;extraRuns=extraRunsUnpaid=silencedCalls=0;
    removedTexts=textsLeft=0;exceptionType=nullptr;
    for(auto& s:silencedScripts){s.original=nullptr;s.native=false;s.passed=s.silenced=0;}
    experienceInstallTried=experienceInstalled=false;experienceCapture=nullptr;experienceCount=nullptr;
    trackerCaptures=trackerCounts=xpCallsAtCapture=0;trackerCountedAfterOriginal=true;
    namespace C=ForgePact::CombatText;
    C::original=nullptr;C::installTried=C::installed=C::native=false;C::xpMultiplier=1;C::rewardScopeActive=nullptr;
    C::remembered=nullptr;C::passedCalls=C::extraRunCalls=C::rememberOverflow=0;
    builtins=originalCalls=stepCalls=cloneCalls=installed=0;
    failClone=aliasClone=failSet=failRead=throwDrop=throwStep=recurse=false;
    nativeAvailable=true;observed=nestedObserved=-1;logs.clear();category=14;executeReward=true;params=MakeParams();
    nodeVars={{"hp",RValue(1.0)},{"miningQue",RValue(0.0)},{"miningActivateDistance",RValue(16.0)},{"stop",RValue(0.0)}};
    instanceWrites=0;failInstanceSet=false;staleAfterFirstRun=false;throwOnStep=throwAfterTextOnStep=paidRuns=dispatches=0;drops.clear();
    std::fill(std::begin(nativeScriptCalls),std::end(nativeScriptCalls),0);floatingTexts.clear();
    liveInstances.clear();nextInstanceId=1000;textDestroys=otherDestroys=textFieldSets=otherObjectAfterStep=0;
    textObjectUnknown=false;maxLiveTexts=0;escaped=false;
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
    // XP, guild XP and quests: once per dig, silenced and counted inside extra runs. The floating
    // text runs in every run and the extra runs' texts are removed, so one is left per dig.
    Reset();RollsCommand("3");RunStep();
    bool passedOnce=true, silencedTwice=true;
    for(const auto& s:silencedScripts){passedOnce=passedOnce&&s.passed==1;silencedTwice=silencedTwice&&s.silenced==2;}
    Check(!escaped&&ScriptsOnce()&&passedOnce&&silencedTwice&&silencedCalls==8&&ForgePact::CombatText::extraRunCalls==2
        &&ForgePact::CombatText::passedCalls==1&&removedTexts==2&&SilencedCalls()==10&&nativeScriptCalls[4]==3,
        "target/xp_silenced_only_inside_extra_runs");
    CallScript(0);UseText(CallScript(4,"7 XP"));
    Check(nativeScriptCalls[0]==2&&nativeScriptCalls[4]==4&&SilencedCalls()==10&&LiveTexts().size()==2,
        "target/xp_silenced_only_inside_extra_runs");

    // ===== ForgePact #222: the dig uses the instance CombatText returns =====
    // The vanilla dig gets a live text and sets its font, with the shared detour in or not.
    Reset();RollsCommand("1");RunStep();
    const bool vanillaText=!escaped&&textFieldSets==1&&LiveTexts()==std::vector<std::string>{"10 XP"};
    Reset();ForgePact::CombatText::Install();RunStep();
    Check(vanillaText&&!escaped&&textFieldSets==1&&LiveTexts()==std::vector<std::string>{"10 XP"}&&textDestroys==0
        &&ForgePact::CombatText::passedCalls==1,"baseline/rolls1_step_uses_combattext_return");
    // Each extra run's step gets a live text of its own, so it does not raise the runner's error.
    Reset();RollsCommand("3");RunStep();
    Check(!escaped&&stepCalls==3&&AllDrops(3,2)&&textFieldSets==3&&nativeScriptCalls[4]==3&&extraRuns==2
        &&rolls==3&&rollsReady&&!rollsUnavailable&&!Logged("miningrolls: an extra roll failed"),
        "target/extra_roll_combattext_return_used");
    // ...and each is removed once its run is over: the original run's text (the first made) stays,
    // and no more than two were ever on screen at once.
    Check(LiveTexts()==std::vector<std::string>{"10 XP"}&&liveInstances.count(1000)&&textDestroys==2&&otherDestroys==0
        &&removedTexts==2&&textsLeft==0&&maxLiveTexts==2&&ForgePact::CombatText::remembered==nullptr,
        "target/extra_roll_texts_removed");
    Reset();RollsCommand("10");RunStep();
    Check(!escaped&&LiveTexts().size()==1&&textDestroys==9&&removedTexts==9&&maxLiveTexts==2,"target/extra_roll_texts_removed");
    // Negative controls: an instance of another object is left alone and counted, and so is every
    // text while Combat_Text_obj cannot be resolved by name.
    Reset();RollsCommand("3");otherObjectAfterStep=1;RunStep();
    Check(!escaped&&textFieldSets==3&&otherDestroys==0&&textDestroys==0&&removedTexts==0&&textsLeft==2
        &&liveInstances.size()==3,"target/extra_roll_other_object_left_alone");
    Reset();RollsCommand("3");textObjectUnknown=true;RunStep();
    Check(!escaped&&textDestroys==0&&removedTexts==0&&textsLeft==2&&LiveTexts().size()==3,
        "target/extra_roll_text_object_unknown_destroys_nothing");
    // An exception in an extra run never reaches the game: the rolls stop, hp ends 0, the rolls go
    // off for the session with one line naming the exception's type, and it is never retried.
    Reset();RollsCommand("3");throwOnStep=2;exceptionType=FakeExceptionType;RunStep();
    Check(!escaped&&stepCalls==2&&drops.size()==1&&extraRuns==1&&nodeVars["hp"].number==0&&!inExtraRun
        &&ForgePact::CombatText::remembered==nullptr&&activeNode==nullptr&&!inReward&&rolls==1&&!rollsReady
        &&rollsUnavailable&&!rollsFailure.empty()&&CountLogged("miningrolls: an extra roll failed")==1
        &&Logged("class std::runtime_error")&&Logged("miningrolls: node left diggable, hp reset to 0"),
        "target/extra_run_exception_stops_rolls_no_rethrow");
    {
        const int installsBefore=installed;RunStep();
        Check(!escaped&&stepCalls==3&&drops.size()==2&&extraRuns==1&&CountLogged("miningrolls: an extra roll failed")==1,
            "target/extra_run_exception_stops_rolls_no_rethrow");
        logs.clear();RollsCommand("3");
        Check(rolls==1&&installed==installsBefore&&CountLogged("miningrolls: unavailable - ")==1,
            "target/extra_run_exception_stops_rolls_no_rethrow");
    }
    // A throw after the run made its text: the text is still removed; with no type source the line says so.
    Reset();RollsCommand("3");throwAfterTextOnStep=2;RunStep();
    Check(!escaped&&stepCalls==2&&drops.size()==2&&LiveTexts()==std::vector<std::string>{"10 XP"}&&textDestroys==1
        &&removedTexts==1&&nodeVars["hp"].number==0&&rolls==1&&rollsUnavailable
        &&CountLogged("miningrolls: an extra roll failed")==1&&Logged("unknown type"),
        "target/extra_run_exception_stops_rolls_no_rethrow");
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
    // Every run's text is rescaled; the one left on screen is the original run's.
    const std::vector<std::string> threeRescaled{"20 XP","20 XP","20 XP"}, oneRescaled{"20 XP"};
    Reset();ForgePact::CombatText::xpMultiplier=2;ForgePact::CombatText::Install();RollsCommand("3");RunStep();
    Check(!escaped&&rollsReady&&installsByName["CombatText"]==1&&ForgePact::CombatText::native&&floatingTexts==threeRescaled
        &&LiveTexts()==oneRescaled&&ForgePact::CombatText::extraRunCalls==2&&removedTexts==2&&AllDrops(3,2),
        "target/combattext_shared_with_xp_multiplier");
    Reset();RollsCommand("3");ForgePact::CombatText::xpMultiplier=2;const bool xpNative=ForgePact::CombatText::Install();RunStep();
    Check(!escaped&&rollsReady&&xpNative&&installsByName["CombatText"]==1&&floatingTexts==threeRescaled&&LiveTexts()==oneRescaled
        &&ForgePact::CombatText::extraRunCalls==2&&removedTexts==2&&AllDrops(3,2),"target/combattext_rolls_first_then_xp");
    Reset();ForgePact::CombatText::rewardScopeActive=[]{return true;};ForgePact::CombatText::xpMultiplier=2;
    ForgePact::CombatText::Install();RunStep();
    Check(floatingTexts.size()==1&&floatingTexts[0]=="10 XP","baseline/combattext_reward_scope_leaves_text");

    // ===== ExperienceUpdate's one detour, shared with the tracker (ForgePact #185) =====
    // Nothing installs it until one of its two consumers asks.
    Reset();
    Check(std::string(ExperienceRoute())=="not-installed"&&installsByName.empty(),"baseline/xp_detour_not_installed_until_asked");
    // The tracker turns on first: the one native detour goes in, and the extra
    // rolls find it there rather than installing a second (table-only) one.
    Reset();const bool trackerFirst=InstallExperience();TrackerAttach();RollsCommand("3");RunStep();
    Check(trackerFirst&&rollsReady&&!rollsUnavailable&&installsByName["ExperienceUpdate"]==1&&silencedScripts[1].native
        &&std::string(ExperienceRoute())=="both"&&silencedScripts[1].passed==1&&silencedScripts[1].silenced==2
        &&trackerCaptures==1&&trackerCounts==1&&trackerCountedAfterOriginal&&nativeScriptCalls[1]==1,
        "target/tracker_first_then_rolls_share_one_native_xp_detour");
    // The other order: the rolls install it, the tracker finds it in.
    Reset();RollsCommand("3");const bool rollsFirst=InstallExperience();TrackerAttach();RunStep();
    Check(rollsFirst&&rollsReady&&installsByName["ExperienceUpdate"]==1&&trackerCaptures==1&&trackerCounts==1
        &&trackerCountedAfterOriginal&&silencedScripts[1].silenced==2,"target/rolls_first_then_tracker_share_one_native_xp_detour");
    // The tracker alone: each call counted once, after the game's own call.
    Reset();InstallExperience();TrackerAttach();CallScript(1);CallScript(1);
    Check(trackerCaptures==2&&trackerCounts==2&&trackerCountedAfterOriginal&&nativeScriptCalls[1]==2
        &&silencedScripts[1].passed==2&&silencedScripts[1].silenced==0&&rolls==1,"target/tracker_alone_counts_every_xp_call_after_the_original");
    // A shared detour that came up table-only still keeps the rolls unavailable, and says which.
    Reset();tableOnly.insert("ExperienceUpdate");InstallExperience();RollsCommand("3");
    Check(rolls==1&&rollsUnavailable&&Logged("miningrolls: unavailable - ExperienceUpdate came up table-only")
        &&std::string(ExperienceRoute())=="table-only"&&installsByName["ExperienceUpdate"]==1,
        "target/a_table_only_shared_xp_detour_keeps_rolls_unavailable");

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
    Reset();Command("5");throwDrop=true;RunStep();
    Check(escaped&&originalCalls==1&&activeNode==nullptr&&!inReward,"target/native_exception_no_retry_scope_restored");
    Reset();Command("5");throwStep=true;RunStep();
    Check(escaped&&stepCalls==1&&activeNode==nullptr,"target/step_exception_scope_restored");
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
    Reset();Command("10");rewardMultiplier=[](CInstance*){return 1;};throwDrop=true;RunStep();
    Check(escaped&&originalCalls==1,"helmet/unequipped native failure never retries");
    Reset();Command("10");rewardMultiplier=[](CInstance*){return 4;};
    rewardDispatched=[](CInstance*,double,double){throw std::runtime_error("effect failure");};RunStep();
    Check(observed==8&&originalCalls==1,"helmet/effect failure never duplicates reward");
    std::cout<<(failures?"RESULT FAILED":"RESULT OK")<<"\n";return failures?1:0;
}
