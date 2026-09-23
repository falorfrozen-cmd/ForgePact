#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdint>
#ifdef MINER_HELMET_BASELINE
namespace ForgePact::MinerRules {
struct Node { int64_t id; double x,y,hp,requirement; bool busy,normal; };
bool Matches(double,double,double,double,double){return false;}
int QuantityMultiplier(bool,bool,int slider){return slider;}
std::vector<Node> Nearby(const std::vector<Node>&,int64_t,double,double,double){return {};}
}
#else
#include <ForgePact/MinerHelmetModel.hpp>
#endif
int main(){
    using namespace ForgePact::MinerRules;
    int failed=0;
    auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';if(!ok)++failed;};
    check(QuantityMultiplier(false,false,1)==1,"baseline/default remains vanilla");
    check(QuantityMultiplier(false,false,5)==5,"baseline/experimental slider independent");
    check(QuantityMultiplier(true,true,10)==4,"helmet/exactly four, no slider stacking");
    check(QuantityMultiplier(true,false,10)==10&&QuantityMultiplier(true,false,1)==1,"helmet/bag or removed falls back to the ore slider");
    check(Matches(0,777003,7,0,0),"helmet/reserved identity recognised");
    check(!Matches(0,777001,7,0,0)&&!Matches(8,777003,7,0,0)&&!Matches(0,777003,7,1,0),"helmet/other signatures excluded");
    std::vector<Node> nodes={{1,0,0,1,1,false,true},{2,30,0,1,20,false,true},{3,20,0,1,10,false,true},
        {4,50,0,1,999,false,true},{5,60,0,0,1,false,true},{6,70,0,1,1,true,true},
        {7,80,0,1,1,false,false},{8,500,0,1,1,false,true},{9,90,0,1,1,false,true}};
    auto picked=Nearby(nodes,1,0,0,100);
    check(picked.size()==2&&picked[0].id==3&&picked[1].id==2,"wave/nearest two only, excludes root and blocked veins");
    check(Nearby(nodes,1,0,0,0).empty(),"wave/mining level requirement respected");
    nodes={{2,10,0,1,1,false,true},{2,10,0,1,1,false,true},{3,15,0,1,1,false,true}};
    picked=Nearby(nodes,1,0,0,100);
    check(picked.size()==2&&picked[0].id!=picked[1].id,"wave/no duplicate target");
    check(Nearby(nodes,1,NAN,0,100).empty()&&Nearby(nodes,1,0,0,NAN).empty(),"wave/unreadable origin or skill refuses");
    std::cout<<(failed?"RESULT FAILED":"RESULT OK")<<'\n';return failed?1:0;
}
