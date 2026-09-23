#include <ForgePact/AdaptivePopulationBudget.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

static uint64_t nowUs=0;
static uint64_t clockUs(){return nowUs;}
static void require(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}

int main(){
    ForgePact::AdaptivePopulationBudget budget(&clockUs);
    budget.BeginFrame(0,false);
    require(!budget.Active(),"disabled baseline");
    budget.BeginPass(1536);
    // 384 original groups + exactly three copies each. Copies get the same
    // delayed first native poll as other creators; a rejected poll retries
    // later, not on every frame. This is a timing fixture, not a game benchmark.
    struct Pack {unsigned next;bool done=false;};
    std::vector<Pack> packs;
    for(unsigned n=0;n<384;++n)packs.push_back({1+n%80});
    unsigned copies=1152,done=0,created=0;
    for(unsigned frame=1;frame<=250 && done<1536;++frame){
        nowUs=uint64_t(frame)*20000;
        budget.ObserveBacklog(packs.size()-done,copies);
        budget.BeginFrame(frame,true);
        // Ordinary combat already consumes some creation time. It must not
        // collapse the population controller to one permission every 8 frames.
        budget.RecordNative(600);
        while(copies && budget.CanCopy(true)){
            budget.CopyStarted();--copies;++created;
            budget.RecordNative(50,true);nowUs+=50;
            packs.push_back({frame+80});
        }
        for(auto& pack:packs)if(!pack.done && pack.next<=frame){
            if(budget.ReservePack()){
                pack.done=true;++done;budget.RecordNative(150);nowUs+=150;
            }else pack.next=frame+80;
        }
    }
    std::cout<<"five-second fixture: copies="<<created<<" groups="<<done<<" lastUs="<<nowUs<<std::endl;
    require(created==1152 && copies==0,"all 4x copies must be preserved");
    require(done==1536 && nowUs<=5000000,"normal 50fps workload must finish inside five seconds");
    std::cout<<"five-second population PASS\n";
}
