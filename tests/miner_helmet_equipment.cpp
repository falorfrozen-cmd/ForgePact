#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <ForgePact/MinerHelmetModel.hpp>
enum {VALUE_UNDEFINED, VALUE_REAL, VALUE_REF, VALUE_OBJECT, VALUE_STRING, VALUE_ARRAY};
struct Fields;
struct RValue {
    int m_Kind=VALUE_UNDEFINED; double number=0; std::string text;
    std::shared_ptr<Fields> m_Object; std::vector<RValue> array;
    RValue()=default;
    RValue(double n):m_Kind(VALUE_REAL),number(n){}
    RValue(const char* s):m_Kind(VALUE_STRING),text(s){}
    double ToDouble()const{return number;}
    bool ToBoolean()const{return number!=0;}
    std::string ToString()const{return text;}
};
struct Fields {std::map<std::string,RValue> values;};
struct CInstance{};
static CInstance global;
using AurieStatus=int;
static bool AurieSuccess(int status){return status==0;}
namespace HeroSiege::Scripts {
constexpr std::string_view gml_Script_GetOnlinePlayerItemOwner="owner";
constexpr std::string_view gml_Script_GetItemFromFingerprint="item";
}
namespace ForgePact::MiningOre {
bool WholeNumber(const RValue& v,double& d){d=v.number;return v.m_Kind==VALUE_REAL&&std::isfinite(d)&&std::floor(d)==d;}
}
static bool hasPlayer=true,failResolve=false;
static RValue slots, local(1.0), item;
static int resolveCalls=0,reads=0;
static bool HhResolveLocalPlayer(RValue& out){out=RValue(555.0);out.m_Kind=VALUE_REF;return hasPlayer;}
static bool TryStructNumber(const RValue& v,const char* key,double& out){
    if(v.m_Kind!=VALUE_OBJECT||!v.m_Object||!v.m_Object->values.count(key))return false;
    RValue x=v.m_Object->values.at(key);out=x.number;return x.m_Kind==VALUE_REAL;
}
struct Runtime {
    void GetGlobalInstance(CInstance** out){*out=&global;}
    RValue CallBuiltin(const char* name,std::initializer_list<RValue> args){
        ++reads; std::vector<RValue>a(args);std::string n(name);
        if(n=="array_length")return RValue((double)a[0].array.size());
        if(n=="array_get")return a[0].array.at((int)a[1].number);
        if(n=="variable_global_get"){
            if(a[0].text=="mplr")return local;
            if(a[0].text=="equippedItems")return slots;
            throw std::runtime_error("unexpected inventory/global scan");
        }
        if(n=="variable_struct_exists")return RValue((double)a[0].m_Object->values.count(a[1].text));
        if(n=="variable_struct_get")return a[0].m_Object->values.at(a[1].text);
        throw std::runtime_error("unexpected builtin");
    }
    int CallGameScriptEx(RValue& result,const char* name,CInstance*,CInstance*,std::initializer_list<RValue> args){
        std::vector<RValue>a(args);
        if(std::string(name)=="owner"){
            if(a.at(0).number!=1)throw std::runtime_error("wrong player owner");
            result=RValue(0.0);return 0;
        }
        ++resolveCalls;
        if(a.at(1).number!=0||a.at(0).text!="helmet-fingerprint")throw std::runtime_error("wrong slot/fingerprint");
        result=item;return failResolve?1:0;
    }
};
static Runtime runtime;static Runtime* g_Yytk=&runtime;
namespace ForgePact::MinerHelmet {
bool worn=false,equipmentReadable=false;std::string equipmentReason;
// PRODUCTION_EQUIPMENT
}
static RValue Arr(std::initializer_list<RValue> a){RValue v;v.m_Kind=VALUE_ARRAY;v.array=a;return v;}
static RValue Obj(std::map<std::string,RValue> fields){RValue v;v.m_Kind=VALUE_OBJECT;v.m_Object=std::make_shared<Fields>();v.m_Object->values=fields;return v;}
static void Reset(){
    hasPlayer=true;failResolve=false;resolveCalls=reads=0;local=RValue(1.0);
    slots=Arr({RValue(),Arr({Arr({RValue("helmet-fingerprint")}),Arr({})})});
    item=Obj({{"itemType",RValue(0.0)},{"itemDefinitionStruct",Obj({{"a",RValue(777003.0)},
        {"b",RValue(7.0)},{"c",RValue(0.0)},{"j",RValue(0.0)}})}});
}
int main(){
    using namespace ForgePact::MinerHelmet;int failed=0;
    auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';failed+=!ok;};
    Reset();check(ReadWorn()&&equipmentReadable&&resolveCalls==1,"target/worn local helmet resolves actual fingerprint");
    Reset();slots.array[1].array[0].array[0]=RValue();check(!ReadWorn()&&resolveCalls==0,"baseline/helmet only in bag cannot activate");
    Reset();slots.array[1].array[0]=Arr({});check(!ReadWorn()&&!equipmentReadable,"baseline/missing helmet slot refuses");
    Reset();slots.array[1].array[0].array[0]=RValue("");check(!ReadWorn()&&resolveCalls==0,"baseline/empty fingerprint refuses");
    Reset();slots.array[1].array[1]=slots.array[1].array[0];slots.array[1].array[0].array[0]=RValue();check(!ReadWorn(),"baseline/mercenary helmet excluded");
    Reset();slots.array[0]=slots.array[1];slots.array[1].array[0].array[0]=RValue();check(!ReadWorn(),"baseline/another player excluded");
    Reset();item.m_Object->values["itemDefinitionStruct"].m_Object->values["a"]=RValue(777001.0);check(!ReadWorn(),"baseline/tyrant crown excluded");
    Reset();failResolve=true;check(!ReadWorn()&&!equipmentReadable,"baseline/resolver failure refuses");
    Reset();item.m_Object->values["itemDefinitionStruct"].m_Object->values["a"]=RValue(412831.0);
    item.m_Object->values["fp_mechanic"]=RValue("miner");check(ReadWorn(),"target/Custom Forge random-seed miner helmet works");
    item.m_Object->values["itemType"]=RValue(8.0);check(!ReadWorn(),"baseline/miner tag on a belt cannot activate");
    Reset();local=RValue(NAN);check(!ReadWorn()&&resolveCalls==0,"baseline/invalid player index refuses");
    Reset();hasPlayer=false;check(!ReadWorn()&&reads==0,"baseline/menus no player no equipment scan");
    std::cout<<(failed?"RESULT FAILED":"RESULT OK")<<'\n';return failed?1:0;
}
