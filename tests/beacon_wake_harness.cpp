#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#undef assert
#define assert(condition) do { if(!(condition)){std::cerr<<"FAILED: " #condition "\n";return 1;} } while(false)
#define FP_POP_SCOPE(...) ((void)0)
enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_REF, VALUE_OBJECT, VALUE_UNDEFINED };
static int handleKind=VALUE_REAL;
struct RValue {
    int m_Kind=VALUE_REAL;
    double value=0;std::string text;
    RValue(double n):value(n){}
    RValue(const char* s):text(s){}
    double ToDouble()const{return value;}
};
struct Enemy {int id,rarity;double x,y;bool active;};
static std::vector<Enemy> enemies;
static long finds=0,reads=0,activations=0;
static Enemy& byId(int id){for(auto& e:enemies)if(e.id==id)return e;throw "invalid id";}
static bool HuntWants(const RValue& inst,int policy){return policy==2 || (policy==1 && byId(int(inst.value)).rarity>=2);}
struct Runner {
    RValue CallBuiltin(const char* key,std::vector<RValue> args){
        const std::string name(key);
        if(name=="instance_number")return RValue(std::count_if(enemies.begin(),enemies.end(),[](const Enemy& e){return e.active;}));
        if(name=="instance_activate_object"){++activations;for(auto& e:enemies)e.active=true;return RValue(0.0);}
        if(name=="instance_deactivate_object"){byId(int(args[0].value)).active=false;return RValue(0.0);}
        if(name=="instance_find"){
            ++finds;int remaining=int(args[1].value);
            for(auto& e:enemies)if(e.active && remaining--==0){RValue v(e.id);v.m_Kind=handleKind;return v;}
            throw "invalid ordinal";
        }
        if(name=="variable_instance_get"){
            ++reads;auto& e=byId(int(args[0].value));const auto& field=args[1].text;
            if(field=="id")return RValue(e.id);
            if(field=="x")return RValue(e.x);
            if(field=="y")return RValue(e.y);
        }
        throw "unexpected builtin";
    }
} runner;
static Runner* g_Yytk=&runner;
// The production flag the injected body reads: false until a count change
// proves that something deactivates monsters (the harness flips it to run the
// old filter semantics, which the game itself never reaches).
static bool g_BeWakeSnapshot=false;
// PRODUCTION_WAKE
int main(){
    // The vanilla case (MEASURED 2026-09-22: the game never deactivates
    // monsters): activation changes nothing, so the pass makes no lookup and
    // reads no variable at all - not even a snapshot of the active set.
    handleKind=VALUE_REF;
    for(int id=1;id<=4096;++id)enemies.push_back({id,0,9999,0,true});
    finds=reads=activations=0;
    assert(BeWakeObject(RValue(12),0,0,4000,1)==4096);
    std::cout<<"vanilla lookups="<<finds<<" id reads="<<reads<<"\n";
    assert(finds==0 && reads==0 && activations==1 && !g_BeWakeSnapshot);
    std::cout<<"vanilla: no walk PASS\n";
    // The first time a count change is seen, the newly woken instances stay
    // awake (conservative) and the full walk is armed for later calls.
    enemies={{1,0,9999,0,true},{2,0,0,0,false},{3,2,3,4,false},{4,3,4001,0,false}};
    finds=reads=activations=0;
    assert(BeWakeObject(RValue(12),0,0,4000,1)==4);
    for(const auto& e:enemies)assert(e.active);
    assert(finds==0 && reads==0 && activations==1 && g_BeWakeSnapshot);
    std::cout<<"first count change: left awake, snapshot armed PASS\n";
    // What that call woke stays awake only until whatever put it to sleep does
    // so again; the next pass has the snapshot and filters exactly.
    for(int id:{2,3,4})byId(id).active=false;
    assert(BeWakeObject(RValue(12),0,0,4000,1)==2);   // native 1, plus hunter 3 inside the radius
    assert(byId(1).active && !byId(2).active && byId(3).active && !byId(4).active);
    std::cout<<"after the discovery: a repeated deactivation is filtered exactly PASS\n";
    // From here on the snapshot walk runs. Baseline and target behavior:
    // preserve every native-active instance; add only hunters within the
    // radius, including its exact boundary.
    for(int kind:{VALUE_REAL,VALUE_INT32,VALUE_INT64,VALUE_REF,VALUE_OBJECT})
    for(int policy:{1,2})for(double radius:{-1.0,0.0,5.0,4000.0}){
        handleKind=kind;
        enemies={{1,0,9999,0,true},{2,0,0,0,false},{3,2,3,4,false},{4,3,4001,0,false}};
        auto before=enemies;long expected=0;
        for(const auto& e:before)expected+=e.active || ((policy==2 || e.rarity>=2) && (radius<0 || e.x*e.x+e.y*e.y<=radius*radius));
        assert(BeWakeObject(RValue(12),0,0,radius,policy)==expected);
        for(size_t i=0;i<enemies.size();++i){
            const auto& e=before[i];
            assert(enemies[i].active==(e.active || ((policy==2 || e.rarity>=2) && (radius<0 || e.x*e.x+e.y*e.y<=radius*radius))));
        }
        assert(BeWakeObject(RValue(12),0,0,radius,policy)==expected);
    }
    handleKind=VALUE_REF;
    enemies.clear();assert(BeWakeObject(RValue(12),0,0,4000,1)==0);
    std::cout<<"wake behavior PASS\n";
    // Population already active: a second identical lookup walk adds no
    // information. Guard counts, not mock wall-clock timings.
    for(int id=1;id<=4096;++id)enemies.push_back({id,0,9999,0,true});
    finds=reads=activations=0;
    assert(BeWakeObject(RValue(12),0,0,4000,1)==4096);
    std::cout<<"already active lookups="<<finds<<" id reads="<<reads<<"\n";
    assert(finds<=4096 && reads==0 && activations==1);
    // Mixed activation must preserve native-active ordinary enemies, even
    // outside the hunt radius. Only newly activated unwanted enemies sleep.
    // IDs deliberately differ from list positions and enumeration order.
    enemies.clear();
    for(int i=0;i<4096;++i)enemies.push_back({100000+(4095-i)*3,i%4,double(i%5000),0,i%3==0});
    auto mixed=enemies;finds=reads=activations=0;
    long mixedExpected=0;
    for(const auto& e:mixed)mixedExpected+=e.active || (e.rarity>=2 && e.x<=2000);
    assert(BeWakeObject(RValue(12),0,0,2000,1)==mixedExpected);
    for(size_t i=0;i<mixed.size();++i)assert(enemies[i].active==(mixed[i].active || (mixed[i].rarity>=2 && mixed[i].x<=2000)));
    // The only variable reads are positions for newly activated hunters.
    long positionReads=0;
    for(const auto& e:mixed)if(!e.active && e.rarity>=2)positionReads+=2;
    assert(reads==positionReads && activations==1);
    std::cout<<"mixed activation preserves native set with no redundant id reads PASS\n";
    for(auto& e:enemies)e.active=false;
    finds=reads=activations=0;
    assert(BeWakeObject(RValue(12),0,0,-1,2)==4096);
    assert(finds==0 && reads==0 && activations==1);
    std::cout<<"wake redundant walks removed PASS\n";
}
