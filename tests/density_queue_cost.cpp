#include <ForgePact/DeferredDensityCopies.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <set>
#include <vector>
#include <limits>

static size_t coordinateReads=0;
struct Coordinate {
    double value=0;
    Coordinate(double n=0):value(n){}
    operator double()const {++coordinateReads;return value;}
};
struct Recipe {Coordinate x,y;};
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    std::cout<<std::unitbuf;
    ForgePact::DeferredDensityCopies<int,Recipe> queue;
    for(int i=0;i<4096;++i)check(queue.Schedule(i,{double(i),0},1),"schedule");
    coordinateReads=0;
    for(int i=0;i<32;++i){
        auto job=queue.TakeNearest(0,0,1);
        check(job && job->key==i,"exact nearest order");queue.Complete(*job);
    }
    std::cout<<"4096 placements / 32 selections: coordinate reads="<<coordinateReads<<"\n";
    check(coordinateReads<40000,"selection repeats a full scan for every copy");
    // Moving the player must invalidate distances without losing jobs.
    auto far=queue.TakeNearest(5000,0,2);check(far && far->key==4095,"moving player");
    queue.Retry(*far,100);
    auto next=queue.TakeNearest(5000,0,3);check(next && next->key==4094,"retry stays asleep");
    queue.Complete(*next);
    auto due=queue.TakeNearest(5000,0,100);check(due && due->key==4095,"retry wakes on time");
    queue.Complete(*due);
    check(queue.Schedule(6000,{5000,0},1),"late schedule");
    auto late=queue.TakeNearest(5000,0,100);check(late && late->key==6000,"new nearby job");
    queue.Complete(*late);
    // Dense equal-position copies are distinct; ties are stable by insertion.
    queue.Reset();queue.Schedule(7,{10,10},3);
    for(unsigned i=1;i<=3;++i){auto j=queue.TakeNearest(0,0,0);check(j && j->ordinal==i,"equal position tie");queue.Complete(*j);}
    check(!queue.TakeNearest(0,0,0),"drained");
    // A future retry cannot block ready work, and old-zone retries cannot return.
    queue.Reset();queue.Schedule(1,{0,0},1);queue.Schedule(2,{5,0},1);
    auto old=queue.TakeNearest(0,0,1);queue.Retry(*old,1000);
    check(queue.TakeNearest(0,0,2)->key==2,"ready sibling");
    queue.NewZone();queue.Retry(*old,3);check(queue.Pending()==0,"stale retry");
    queue.Schedule(1,{0,0},1);check(queue.TakeNearest(0,0,4)->key==1,"resumed placement");
    // Compare a moving player and delayed contexts against an independent
    // linear closest-point oracle, including fully-drained waiting periods.
    queue.Reset();
    struct Expected{double x,y;bool done=false;uint64_t retryAt=0;};
    std::vector<Expected> expected;
    for(int i=0;i<500;++i){
        double x=(i*17)%997,y=(i*29)%991;
        expected.push_back({x,y});queue.Schedule(i,{x,y},1);
    }
    size_t completed=0;
    for(uint64_t frame=0;frame<4000 && completed<expected.size();++frame){
        const double px=((frame/7)*37)%1200,py=((frame/7)*43)%1100;
        int nearest=-1;double best=std::numeric_limits<double>::infinity();
        for(int i=0;i<int(expected.size());++i){
            const auto& e=expected[i];if(e.done || e.retryAt>frame)continue;
            const double dx=e.x-px,dy=e.y-py,d=dx*dx+dy*dy;
            if(d<best){nearest=i;best=d;}
        }
        auto job=queue.TakeNearest(px,py,frame);
        check(bool(job)==(nearest>=0),"oracle ready/deferred agreement");
        if(!job)continue;
        check(job->key==nearest,"oracle nearest agreement");
        auto& e=expected[nearest];
        if(frame%5==0 && frame<1000){e.retryAt=frame+11;queue.Retry(*job,e.retryAt);}
        else {e.done=true;++completed;queue.Complete(*job);}
    }
    check(completed==500 && queue.Pending()==0,"every oracle job completes exactly once");
    std::cout<<"density queue cost and behavior PASS\n";
}
