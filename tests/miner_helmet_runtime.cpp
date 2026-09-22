// Full production helmet + ore adapters against a deterministic fake runner.
// Does not load the game, install a DLL, or read/write character saves.
#include <algorithm>
#include <cmath>
#include <cstdint>
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
struct Fields;
struct RValue {
    int m_Kind=VALUE_UNDEFINED;
    double number=0;
    std::string text;
    std::shared_ptr<Fields> m_Object;
    std::vector<RValue> array;
    RValue()=default;
    RValue(double n):m_Kind(VALUE_REAL),number(n){}
    RValue(const char* s):m_Kind(VALUE_STRING),text(s){}
    explicit RValue(bool b):m_Kind(VALUE_BOOL),number(b){}
    double ToDouble() const {return number;}
    int64_t ToInt64() const {return (int64_t)number;}
    bool ToBoolean() const {return number!=0;}
    std::string ToString() const {return text;}
};
struct Fields {std::map<std::string,RValue> values;};
static RValue Obj(std::map<std::string,RValue> fields){
    RValue v;v.m_Kind=VALUE_OBJECT;v.m_Object=std::make_shared<Fields>();v.m_Object->values=fields;return v;
}
static RValue Arr(std::initializer_list<RValue> a){RValue v;v.m_Kind=VALUE_ARRAY;v.array=a;return v;}
struct CInstance {
    int64_t id;
    RValue ToRValue() const {RValue v((double)id);v.m_Kind=VALUE_REF;return v;}
};
static CInstance global{0},player{100},node{200},other{300};
using AurieStatus=int;
using PVOID=void*;
using PFUNC_YYGMLScript=RValue&(*)(CInstance*,CInstance*,RValue&,int,RValue**);
static bool AurieSuccess(int s){return s==0;}
static bool hasPlayer=true, readable=true, nativeAvailable=true, failEffect=false;
static bool pointerResolverAvailable=true, typedInstanceIds=false;
static double now=10000, alpha=0.75, colour=123, cameraWidth=1000;
static int64_t room=8,miner=100;
static int nativeCalls=0,scriptCalls=0,equipmentQueries=0,drawCalls=0,grants=0,installCalls=0;
static bool failNative=false;
static double receivedOre=0, playerX=50, playerY=50, nodeX=50, nodeY=50;
static int minerKind=VALUE_REF;   // what the node's miningPlayer holds: a reference, or a plain real such as noone
static int assetLookups=0, nearestCalls=0, variableSets=0;
static std::map<std::string,RValue> nodeVars;   // the node's slider state as the probe reads it
// Other mining nodes of the zone (vein resonance), each with its own variables.
struct FakeNode { CInstance inst; std::map<std::string,RValue> vars; };
static std::vector<FakeNode> zoneNodes;
static std::map<double,double> protectedValues;   // GPV handle -> value
static RValue miningLevel;
static FakeNode* ZoneNode(double id){ for(auto& z:zoneNodes) if((double)z.inst.id==id) return &z; return nullptr; }
static void AddZoneNode(int64_t id,double x,double y,double hp=1,bool active=false,bool que=false,double req=1){
    FakeNode z; z.inst.id=id;
    z.vars={{"x",RValue(x)},{"y",RValue(y)},{"hp",RValue(hp)},{"miningActive",RValue(active)},{"miningQue",RValue(que)},
        {"miningActivateDistance",RValue(16.0)},{"miningReq",RValue((double)id*10)},{"miningPlayer",RValue(-4.0)}};
    protectedValues[(double)id*10]=req; zoneNodes.push_back(z);
}
static RValue equipment,helmet,params;
static std::vector<std::string> logs;
static double HhNowMs(){return now;}
static int64_t CurrentRoomKey(){return room;}
static void Out(const std::string& s){logs.push_back(s);}
static bool HhResolveLocalPlayer(RValue& out){out=player.ToRValue();return hasPlayer;}
static CInstance* HhResolveInstance(const RValue& value){
    if(!pointerResolverAvailable)return nullptr;
    if(value.m_Kind!=VALUE_REF)return nullptr;
    if(value.number==player.id&&hasPlayer)return &player;
    if(value.number==node.id)return &node;
    if(value.number==other.id)return &other;
    return nullptr;
}
static bool TryStructNumber(const RValue& object,const char* key,double& number){
    if(object.m_Kind!=VALUE_OBJECT||!object.m_Object)return false;
    auto it=object.m_Object->values.find(key);if(it==object.m_Object->values.end())return false;
    if(it->second.m_Kind!=VALUE_REAL&&it->second.m_Kind!=VALUE_INT32&&it->second.m_Kind!=VALUE_INT64&&it->second.m_Kind!=VALUE_BOOL)return false;
    number=it->second.number;return std::isfinite(number);
}
static consteval const char* SdkShortScriptName(std::string_view n){return n.data()+11;}
static bool HookOneScript(const char*,const char*,PVOID,PFUNC_YYGMLScript*,bool*);
static void InstallHeadLabelHook(){}
static PFUNC_YYGMLScript g_Orig_DrawHudBuffs=(PFUNC_YYGMLScript)1;
static void* GetModuleHandleA(const char*){return nullptr;}
static bool AddrIsExecutableInModule(void*,const void*){return false;}
static bool SpawnSignatureItem(int which,double,double,CInstance*){++grants;return which==2;}

struct Runtime {
    void GetGlobalInstance(CInstance** result){*result=&global;}
    RValue CallBuiltin(const char* name,std::initializer_list<RValue> arguments){
        const std::string n(name);std::vector<RValue>a(arguments);
        if(n=="variable_global_get"){
            if(a[0].text=="mplr")return RValue(1.0);
            if(a[0].text=="equippedItems"){++equipmentQueries;return equipment;}
        }
        if(n=="array_length")return RValue((double)a[0].array.size());
        if(n=="array_get")return a[0].array.at((size_t)a[1].number);
        if(n=="variable_struct_exists")return RValue(a[0].m_Object&&a[0].m_Object->values.count(a[1].text)>0);
        if(n=="variable_struct_get"){
            if(!a[0].m_Object)return RValue();
            auto it=a[0].m_Object->values.find(a[1].text);
            return it==a[0].m_Object->values.end()?RValue():it->second;
        }
        if(n=="variable_struct_set"){a[0].m_Object->values[a[1].text]=a[2];return RValue();}
        if(n=="variable_clone"){RValue result=a[0];result.m_Object=std::make_shared<Fields>(*a[0].m_Object);return result;}
        if(n=="instance_exists"){
            // A reference or a real instance id (>= 100000); a small real is an object index here and never matches.
            const bool asInstance=a[0].m_Kind==VALUE_REF||(a[0].m_Kind==VALUE_REAL&&a[0].number>=100000);
            return RValue(asInstance&&((a[0].number==player.id&&hasPlayer)||a[0].number==node.id||a[0].number==other.id||ZoneNode(a[0].number)));
        }
        if(n=="instance_number")return RValue((double)(zoneNodes.size()+1));
        if(n=="instance_find"){
            const size_t i=(size_t)a[1].number;
            if(i==0)return node.ToRValue();
            if(i-1<zoneNodes.size())return zoneNodes[i-1].inst.ToRValue();
            return RValue(-4.0);
        }
        if(n=="variable_instance_exists"){
            if(auto z=ZoneNode(a[0].number))return RValue(z->vars.count(a[1].text)>0);
            return RValue(a[1].text!="miningPlayer"||readable);
        }
        if(n=="variable_instance_set"){
            ++variableSets;
            if(auto z=ZoneNode(a[0].number)){z->vars[a[1].text]=a[2];return RValue();}
            if(a[0].number==node.id){nodeVars[a[1].text]=a[2];return RValue();}
            throw std::runtime_error("variable_instance_set on an unknown instance");
        }
        if(n=="variable_instance_get"){
            if(a[1].text=="id"){
                RValue id(a[0].number);if(typedInstanceIds)id.m_Kind=VALUE_REF;return id;
            }
            if(auto z=ZoneNode(a[0].number)){auto it=z->vars.find(a[1].text);return it==z->vars.end()?RValue():it->second;}
            if(a[1].text=="miningPlayer"){
                if(!readable)throw std::runtime_error("missing miningPlayer");
                RValue v((double)miner);v.m_Kind=minerKind;return v;
            }
            if(a[1].text=="x")return RValue(a[0].number==player.id?playerX:nodeX);
            if(a[1].text=="y")return RValue(a[0].number==player.id?playerY:nodeY);
            if(a[0].number==node.id){auto it=nodeVars.find(a[1].text);if(it!=nodeVars.end())return it->second;}
        }
        if(n=="asset_get_index"){++assetLookups;return RValue(77.0);}
        if(n=="instance_nearest"){++nearestCalls;return node.ToRValue();}
        if(n=="view_get_camera")return RValue(1.0);
        if(n=="camera_get_view_x"||n=="camera_get_view_y")return RValue(0.0);
        if(n=="camera_get_view_width")return RValue(cameraWidth);
        if(n=="camera_get_view_height"||n=="display_get_gui_height"||n=="display_get_gui_width")return RValue(1000.0);
        if(n=="draw_get_colour")return RValue(colour);
        if(n=="draw_get_alpha")return RValue(alpha);
        if(n=="draw_set_colour"){colour=a[0].number;return RValue();}
        if(n=="draw_set_alpha"){alpha=a[0].number;return RValue();}
        if(n=="make_colour_rgb")return RValue(456.0);
        if(n=="draw_ellipse"){
            ++drawCalls;
            for(size_t i=0;i<4;++i)if(!std::isfinite(a[i].number))throw std::runtime_error("bad projection");
            if(failEffect)throw std::runtime_error("drawing failed");return RValue();
        }
        throw std::runtime_error("unexpected builtin: "+n);
    }
    RValue CallGameScript(const char* name,std::initializer_list<RValue> arguments){
        std::vector<RValue>a(arguments);std::string n(name);
        if(n=="gml_Script_GetMiningLevel")return miningLevel;
        if(n=="gml_Script_GPV"){auto it=protectedValues.find(a.at(0).number);return it==protectedValues.end()?RValue():RValue(it->second);}
        throw std::runtime_error("unexpected game script: "+n);
    }
    int CallGameScriptEx(RValue& result,const char* name,CInstance*,CInstance*,std::initializer_list<RValue> arguments){
        ++scriptCalls;std::vector<RValue>a(arguments);std::string n(name);
        if(n==HeroSiege::Scripts::gml_Script_GetOnlinePlayerItemOwner){result=RValue(0.0);return 0;}
        if(n==HeroSiege::Scripts::gml_Script_GetItemFromFingerprint){
            if(a[0].m_Kind!=VALUE_STRING)throw std::runtime_error("invalid fingerprint entered game script");
            result=helmet;return 0;
        }
        throw std::runtime_error("unexpected game script: "+n);
    }
};
static Runtime runtime;static Runtime* g_Yytk=&runtime;
#include <ForgePact/MiningOreMod.hpp>
#include <ForgePact/MinerHelmetState.hpp>
#include <ForgePact/MinerHelmetMod.hpp>

static RValue& NativeDrop(CInstance*,CInstance*,RValue& r,int,RValue** args){
    ++nativeCalls;receivedOre=args[3]->m_Object->values.at("o").number;
    if(failNative)throw std::runtime_error("native reward failed");
    r=RValue(999.0);return r;
}
static RValue& NativeStep(CInstance* s,CInstance* o,RValue& r,int,RValue**){
    RValue x(80.0),y(100.0),type(14.0);RValue* args[]={&x,&y,&type,&params};
    return ForgePact::MiningOre::HookLoot(s,o,r,4,args);
}
static bool HookOneScript(const char* name,const char*,PVOID,PFUNC_YYGMLScript* orig,bool* native){
    ++installCalls;*native=nativeAvailable;
    *orig=std::string(name)=="MiningNodeStepMain"?NativeStep:NativeDrop;return true;
}
static void Mine(){RValue r;ForgePact::MiningOre::HookStep(&node,&node,r,0,nullptr);}
static void MineAt(CInstance* s){RValue r;ForgePact::MiningOre::HookStep(s,s,r,0,nullptr);}
static void Reset(){
    namespace M=ForgePact::MinerHelmet;namespace O=ForgePact::MiningOre;
    M::pending=M::enabled=M::worn=M::equipmentReadable=M::hudNative=false;
    M::waves.clear();M::recentNodes.clear();M::grantRequests.clear();M::grantRequest.clear();
    M::rewards=M::wavesStarted=0;M::lastStatus=0;M::lastGrant=-10000;
    M::lastRewardReason="No mining ore reward observed";M::rewardRefusalsLogged=0;
    M::probeUntil=0;M::probeLines=M::probeFrames=M::probeRewardLines=0;M::probeLast.clear();
    assetLookups=nearestCalls=variableSets=0;nodeVars={{"stop",RValue(0.0)},{"range",RValue(12.0)},{"rangeMax",RValue(100.0)}};
    M::resonating.clear();M::veinResonance=true;M::resonanceFrame=M::resonanceStarted=M::resonanceExpired=M::resonanceLogs=0;M::bonusVeins=0;M::lastRewardWasHelmet=false;
    zoneNodes.clear();protectedValues.clear();miningLevel=RValue(10.0);
    M::equipmentReason="No helmet loaded";
    O::multiplier=1;O::installTried=O::ready=O::unavailable=false;
    O::loggedReward=O::loggedFailure=false;O::rewardMultiplier=nullptr;O::rewardDispatched=nullptr;
    O::stepObserved=O::oreObserved=false;
    O::activeNode=nullptr;O::inReward=false;O::originalStep=NativeStep;O::originalLoot=NativeDrop;
    hasPlayer=readable=nativeAvailable=pointerResolverAvailable=true;
    typedInstanceIds=failEffect=failNative=false;
    now=10000;room=8;node.id=200;miner=player.id;minerKind=VALUE_REF;playerX=playerY=nodeX=nodeY=50;alpha=0.75;colour=123;cameraWidth=1000;
    nativeCalls=scriptCalls=equipmentQueries=drawCalls=grants=installCalls=0;receivedOre=0;logs.clear();
    equipment=Arr({RValue(),Arr({Arr({RValue("helmet-fingerprint")}),Arr({})})});
    helmet=Obj({{"itemType",RValue(0.0)},{"itemDefinitionStruct",Obj({
        {"a",RValue(777003.0)},{"b",RValue(7.0)},{"c",RValue(0.0)},{"j",RValue(0.0)}})}});
    params=Obj({{"b",RValue(27.0)},{"o",RValue(5.0)}});
}
int main(){
    namespace M=ForgePact::MinerHelmet;namespace O=ForgePact::MiningOre;
    int failures=0;auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';failures+=!ok;};
    Reset();for(int i=0;i<1000;++i){now+=17;M::Tick();M::Draw();}Mine();
    check(receivedOre==5&&nativeCalls==1&&installCalls==0&&scriptCalls==0&&equipmentQueries==0&&drawCalls==0
        &&!O::stepObserved&&!O::oreObserved,"inactive/1000 frames no hooks or equipment reads; vanilla ore");
    Reset();M::pending=true;M::Tick();Mine();M::Draw();
    check(M::worn&&receivedOre==20&&nativeCalls==1&&params.m_Object->values["o"].number==5&&grants==0
        &&O::stepObserved&&O::oreObserved&&M::lastRewardReason=="4x ore reward dispatched",
        "equipped/one native reward 5 to 20; no auto grant");
    check(drawCalls==2&&alpha==0.75&&colour==123,"pulse/two gold outlines restore renderer state");
    Mine();check(M::wavesStarted==1&&M::rewards==2,"pulse/one ring per node across multiple ore rewards");
    equipment.array[1].array[0].array[0]=RValue();Mine();
    check(receivedOre==5&&nativeCalls==3&&!M::worn,"unequip/next reward immediately returns to x1");
    Reset();M::pending=true;M::Tick();hasPlayer=false;now+=1001;M::Tick();
    check(!M::worn&&!M::equipmentReadable&&M::equipmentReason.find("Equipped")==std::string::npos,"menu/clears equipped status and reason");
    Reset();M::pending=true;M::Tick();miner=other.id;scriptCalls=equipmentQueries=0;Mine();
    check(receivedOre==5&&scriptCalls==0&&equipmentQueries==0&&M::lastRewardReason=="Ore belongs to another player",
        "ownership/another player no bonus or equipment lookup");
    Reset();M::pending=true;M::Tick();readable=false;Mine();check(receivedOre==5&&nativeCalls==1,"ownership/missing miningPlayer keeps vanilla reward");
    Reset();M::pending=true;M::Tick();pointerResolverAvailable=false;typedInstanceIds=true;Mine();M::Draw();
    check(receivedOre==20&&nativeCalls==1&&M::rewards==1&&drawCalls==2,
        "ownership/valid typed player references do not require raw instance pointer resolution");
    Reset();M::pending=true;M::Tick();pointerResolverAvailable=false;typedInstanceIds=true;miner=other.id;Mine();
    check(receivedOre==5&&M::rewards==0,"ownership/typed remote player is still refused");
    // The ordinary dig on the installed build never names the miner: miningPlayer stays noone (a real, -4).
    Reset();M::pending=true;M::Tick();miner=-4;minerKind=VALUE_REAL;Mine();
    check(receivedOre==20&&M::rewards==1&&M::lastRewardReason=="4x ore reward dispatched","ownership/unnamed miner with the player beside the node grants x4");
    Reset();M::pending=true;M::Tick();miner=-4;minerKind=VALUE_REAL;nodeX=1000;Mine();
    check(receivedOre==5&&M::rewards==0&&M::lastRewardReason.find("px away")!=std::string::npos,"ownership/unnamed miner with the player far away is refused");
    Reset();M::pending=true;M::Tick();miner=0;minerKind=VALUE_REAL;Mine();
    check(receivedOre==20&&M::rewards==1,"ownership/a small real is an object index not a miner so proximity decides");
    Reset();M::pending=true;M::Tick();miner=other.id;nodeX=playerX;Mine();
    check(receivedOre==5&&M::rewards==0,"ownership/named remote miner is refused even beside the node");
    Reset();M::pending=true;M::Tick();nodeX=5000;Mine();
    check(receivedOre==20&&M::rewards==1,"ownership/named local miner needs no proximity");
    Reset();equipment.array[1].array[0].array[0]=RValue(123.0);M::ReadWorn();
    check(!M::worn&&scriptCalls==0&&!M::equipmentReadable,"equipment/non-string fingerprint rejected before game scripts");
    Reset();helmet.m_Object->values["itemDefinitionStruct"].m_Object->values["b"]=RValue(1.5);helmet.m_Object->values["fp_mechanic"]=RValue("miner");
    check(!M::ReadWorn(),"equipment/malformed custom definition fails closed");
    Reset();helmet.m_Object->values["itemDefinitionStruct"].m_Object->values["a"]=RValue(412831.0);helmet.m_Object->values["fp_mechanic"]=RValue("miner");
    M::pending=true;M::Tick();Mine();check(M::worn&&receivedOre==20,"custom forge/fresh seed helmet grants x4");
    helmet.m_Object->values["itemType"]=RValue(8.0);Mine();check(receivedOre==5,"custom forge/miner tag on a belt grants no bonus");
    Reset();M::pending=true;M::Tick();O::multiplier=10;Mine();check(receivedOre==20,"helmet/does not stack with x10 slider");
    equipment.array[1].array[0].array[0]=RValue();logs.clear();Mine();
    check(receivedOre==50&&M::rewards==1&&M::wavesStarted==1&&M::lastRewardReason.find("ore slider x10 applies")!=std::string::npos
        &&std::count_if(logs.begin(),logs.end(),[](const std::string& l){return l.find("ore bonus skipped")!=std::string::npos&&l.find("ore slider x10 applies")!=std::string::npos;})==1,
        "slider/helmet removed hands the reward to the x10 slider with no helmet pulse or counter");
    AddZoneNode(5001,150,50);Mine();
    check(receivedOre==50&&M::resonating.empty()&&M::resonanceStarted==0,"slider/a slider-only dig starts no vein resonance");
    O::multiplier=1;Mine();check(receivedOre==5,"slider/helmet removed and slider at x1 is vanilla");
    Reset();M::pending=true;M::Tick();for(int i=0;i<1000;++i){now+=17;M::Tick();M::Draw();}
    check(nativeCalls==0&&grants==0&&drawCalls==0&&equipmentQueries<=18,"idle/equipped status is throttled and creates no rewards or effects");
    Reset();M::pending=true;M::Tick();Mine();room++;M::Draw();check(M::waves.empty()&&drawCalls==0,"pulse/room change removes old effect");
    Reset();M::pending=true;M::Tick();Mine();now+=551;M::Draw();check(M::waves.empty()&&drawCalls==0,"pulse/expires after 550ms");
    Reset();M::pending=true;M::Tick();Mine();now-=100;M::Draw();check(M::waves.empty()&&drawCalls==0,"pulse/clock rollback does not leave a persistent effect");
    Reset();M::pending=true;M::Tick();for(int i=0;i<200;++i){node.id=200+i;Mine();}
    check(M::waves.size()<=6&&M::recentNodes.size()<=128&&nativeCalls==200,"pulse/bounded effects and per-node history");
    Reset();M::pending=true;M::Tick();Mine();failEffect=true;M::Draw();
    check(nativeCalls==1&&alpha==0.75&&colour==123&&M::waves.empty(),"pulse/render failure restores state without another ore reward");
    Reset();M::pending=true;M::Tick();Mine();cameraWidth=0;M::Draw();check(drawCalls==0&&alpha==0.75&&colour==123,"pulse/invalid camera leaves renderer unchanged");
    Reset();M::pending=true;M::Tick();failNative=true;bool caught=false;try{Mine();}catch(...){caught=true;}
    check(caught&&nativeCalls==1&&M::waves.empty()&&!O::inReward&&!O::activeNode,"native failure/no retry or phantom success pulse");
    Reset();nativeAvailable=false;M::pending=true;M::Tick();Mine();check(!M::enabled&&receivedOre==5,"install failure/no bonus");
    Reset();M::Command("grant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");M::Command("grant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    check(grants==1,"create/replayed request grants only one helmet");
    now+=10;M::Command("grant bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");check(grants==1,"create/rapid second request refused");
    Reset();hasPlayer=false;M::Command("grant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");check(grants==0&&installCalls==0,"create/no player does not queue future item");
    // Dig probe: nothing is looked up until armed; once armed it prints the nearest node only when its state changes, then stops on time.
    Reset();for(int i=0;i<120;++i){now+=17;M::Tick();}
    check(assetLookups==0&&nearestCalls==0&&logs.empty(),"probe/not armed reads nothing");
    M::Command("probe 0");check(logs.size()==1&&logs.back().find("probe [1-120")!=std::string::npos&&M::probeUntil==0,"probe/rejects a bad duration");
    logs.clear();M::Command("probe 2");
    auto probeLines=[&]{size_t n=0;for(const auto& l:logs)if(l.rfind("minerhelm probe: node 200",0)==0)++n;return n;};
    for(int i=0;i<60;++i){now+=17;M::Tick();}
    check(probeLines()==1&&logs[1].find("range=12")!=std::string::npos&&logs[1].find("rangeMax=100")!=std::string::npos&&logs[1].find("dist=0")!=std::string::npos,"probe/one line per distinct node state");
    nodeVars["range"]=RValue(40.0);for(int i=0;i<12;++i){now+=17;M::Tick();}
    check(probeLines()==2&&logs.back().find("range=40")!=std::string::npos,"probe/state change adds a line");
    now+=2100;M::Tick();check(M::probeUntil==0&&logs.back().find("probe finished")!=std::string::npos,"probe/stops when the time is up");
    const size_t before=logs.size();for(int i=0;i<60;++i){now+=17;M::Tick();}check(logs.size()==before,"probe/silent after finishing");
    Reset();M::pending=true;M::Tick();Mine();
    check(std::count_if(logs.begin(),logs.end(),[](const std::string& l){return l.rfind("minerhelm probe: reward from node",0)==0;})==1,"probe/first rewards describe their node");
    // Vein resonance. The dug node sits at (50,50); the zone holds candidate veins around it.
    auto veinsSetup=[&]{
        Reset();M::pending=true;M::Tick();
        AddZoneNode(5001,150,50);                 // 100 px: eligible, nearest
        AddZoneNode(5002,50,200);                 // 150 px: eligible, second
        AddZoneNode(5003,50,230);                 // 180 px: eligible but third nearest -> left alone (max two)
        AddZoneNode(5004,350,50);                 // 300 px: out of reach
        AddZoneNode(5005,60,60,0);                // depleted (hp 0)
        AddZoneNode(5006,70,50,1,true);           // being dug already
        AddZoneNode(5007,50,60,1,false,false,50); // needs mining level 50, player has 10
        AddZoneNode(5008,80,50,1,false,true);     // already queued by the game
    };
    auto queued=[&](int64_t id){auto z=ZoneNode((double)id);return z&&z->vars["miningQue"].ToBoolean()&&z->vars["miningActivateDistance"].number==4096;};
    auto untouched=[&](int64_t id){auto z=ZoneNode((double)id);return z&&z->vars["miningActivateDistance"].number==16;};
    veinsSetup();Mine();
    check(queued(5001)&&queued(5002)&&untouched(5003)&&untouched(5004)&&untouched(5005)&&untouched(5006)&&untouched(5007)&&untouched(5008)
        &&!ZoneNode(5005)->vars["miningQue"].ToBoolean()&&M::resonanceStarted==2&&M::resonating.size()==2,
        "veins/a finished dig queues the two nearest eligible veins only");
    check(receivedOre==20&&M::bonusVeins==0,"veins/the dug node itself is an ordinary 4x reward");
    MineAt(&ZoneNode(5001)->inst);
    check(receivedOre==20&&M::rewards==2&&M::bonusVeins==1&&untouched(5001)&&ZoneNode(5001)->vars["miningQue"].ToBoolean()
        &&M::resonating.size()==1&&untouched(5003)&&M::resonanceStarted==2,
        "veins/a queued vein completing is 4x, is restored and starts no chain");
    for(int i=0;i<95;++i){now+=17;M::Tick();}
    check(M::resonating.empty()&&untouched(5002)&&!ZoneNode(5002)->vars["miningQue"].ToBoolean()&&M::resonanceExpired==1,
        "veins/an unfinished vein is released after the timeout");
    veinsSetup();playerX=playerY=2000;MineAt(&ZoneNode(5001)->inst);
    check(receivedOre==5,"veins/a vein nobody queued still needs the player beside it");
    veinsSetup();Mine();playerX=playerY=2000;MineAt(&ZoneNode(5002)->inst);
    check(receivedOre==20&&M::bonusVeins==1,"veins/a queued vein is authorised without the proximity rule");
    veinsSetup();M::Command("veins 0");Mine();
    check(untouched(5001)&&untouched(5002)&&M::resonating.empty()&&variableSets==0,"veins/switched off leaves every vein alone");
    veinsSetup();miningLevel=RValue();Mine();
    check(untouched(5001)&&untouched(5002)&&M::resonating.empty()&&variableSets==0,"veins/unreadable mining level queues nothing");
    veinsSetup();Mine();room++;M::Tick();
    check(M::resonating.empty()&&queued(5001),"veins/leaving the zone forgets queued veins without touching them");
    veinsSetup();M::Command("veins x");check(logs.back().find("veins 0|1")!=std::string::npos&&M::veinResonance,"veins/bad switch value is refused");
    std::cout<<(failures?"RESULT FAILED":"RESULT OK")<<'\n';return failures?1:0;
}
