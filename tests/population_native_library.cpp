// Optional local test against the user's exact native library, in a separate process.
#include <ForgePact/ProtectedPoolRuntime.hpp>
#include <chrono>
#include <iostream>
#include <source_location>
#ifdef FORGEPACT_NATIVE_DETOURS
#include <MinHook.h>
#define FORGEPACT_RELEASE 1
#define BP_DIAG_INCREMENT(x) ((void)0)
static double (*g_OrigProtGet)(double)=nullptr;
static double g_HeroicMult=1,g_CeilingMult=1;
static const int kSlotCeiling=175,kSlotHeroic=177,kSlotHeroicBoost=178;
// Generated verbatim from ModuleMain.cpp by the Python test driver.
#include "population_drop_getter.inc"
#endif
using namespace ForgePact::ProtectedPool;
static void require(bool yes, std::source_location location=std::source_location::current()) {
    if(!yes)throw std::runtime_error("native pool contract failed at line "+std::to_string(location.line()));
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc>=3);
        HMODULE m=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);require(m!=nullptr);
        auto api=Runtime::Resolve(m);require(api.Complete());api.init();
        require(NativeMemoryCheck(api.memory,0,512)==0);
        for(int i=0;i<32;++i)require(api.alloc(i+0.5)==i);
        int fail=argc>3?_wtoi(argv[3]):0,hooks=0;
        Api entry{Runtime::Init,Runtime::Allocate,Runtime::Get,Runtime::Set,Runtime::Free,Runtime::Undefine,
                  Runtime::Scramble,Runtime::Protect,Runtime::Check,Runtime::CheckMemory};
#ifdef FORGEPACT_NATIVE_DETOURS
        require(MH_Initialize()==MH_OK);
        if(fail==-1) { // Drop-rate getter was installed before population was requested.
            require(MH_CreateHook((void*)api.get,(void*)HookProtGet,(void**)&g_OrigProtGet)==MH_OK);
            require(MH_EnableHook((void*)api.get)==MH_OK);
        }
        entry=api; // Call the real exported addresses, not the router directly.
#endif
        const bool ready=Runtime::Install(m,argv[2],[&](const char* name,void* target,void* replacement,void** original){
            ++hooks;if(hooks==fail)return false;
#ifdef FORGEPACT_NATIVE_DETOURS
            if(std::string(name)=="GetVariable") {
                if(g_OrigProtGet){*original=(void*)g_OrigProtGet;return true;}
                replacement=(void*)HookProtGet;
            }
            if(MH_CreateHook(target,replacement,original)!=MH_OK)return false;
            if(std::string(name)=="GetVariable")g_OrigProtGet=(double(*)(double))*original;
            return MH_EnableHook(target)==MH_OK;
#else
            *original=target;return true;
#endif
        },[](const std::string& s){std::cout<<s<<"\n";});
        std::cout<<"ready="<<ready<<" hooks="<<hooks<<" banks="<<Runtime::router.BankCount()<<" reason="<<Runtime::reason<<"\n";
        if(fail>0) {
            require(!ready && !Runtime::active.load());
            require(hooks==fail);
            require(entry.get(7)==7.5 && entry.alloc(123)==32);
            std::cout<<"partial installation stays native PASS "<<fail<<"\n";return 0;
        }
        require(ready && hooks==10 && Runtime::router.BankCount()==3);
        require(entry.get(7)==7.5);
        constexpr size_t count=262144*3+17;
        for(size_t i=32;i<count;++i) {
            const double id=entry.alloc(double(i)+0.5);
            if(id!=double(i)) throw std::runtime_error("allocation "+std::to_string(i)+" returned "+std::to_string(id));
        }
        for(size_t i=0;i<count;++i)require(entry.get(double(i))==double(i)+0.5);
        require(Runtime::router.BankCount()==4 && entry.memory(0,512)==0);
        for(size_t i:{size_t(0),size_t(262143),size_t(262144),size_t(524288),count-1}) {
            entry.set(double(i),-17.25);entry.scramble(double(i));entry.protect(double(i));
            require(entry.get(double(i))==-17.25 && entry.check(double(i))==0);
            entry.undef(double(i));require(entry.get(double(i))==-696969696969.42);
            entry.free(double(i));require(entry.alloc(42)==double(i));
        }
        for(int cycle=0;cycle<2;++cycle) {
            for(size_t i=0;i<count;++i)entry.free(double(i));
            require(Runtime::router.OverflowLive()==0);
            for(size_t i=0;i<count;++i)require(entry.alloc(double(i))==double(i));
            require(Runtime::router.BankCount()==4);
        }
        require(entry.get(-1)==-696969696969.42);entry.set(-1,0);require(entry.check(-1)==1);
#ifdef FORGEPACT_NATIVE_DETOURS
        entry.set(177,28);entry.set(178,37);entry.set(175,99);entry.set(262144+177,28);
        g_HeroicMult=2;g_CeilingMult=3;
        require(entry.get(177)==56 && entry.get(178)==74 && entry.get(175)==33);
        require(entry.get(262144+177)==28); // Overflow local 177 is not the global drop-rate setting.
        g_HeroicMult=1;g_CeilingMult=1;
        require(entry.get(177)==28 && entry.get(175)==99);
        std::cout<<"real detours + production drop getter PASS preinstalled="<<(fail==-1)<<"\n";
#endif
        const auto begin=std::chrono::steady_clock::now();
        double sum=0;for(size_t i=0;i<1000000;++i)sum+=entry.get(double(i%262144));
        const double routed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        const auto nativeBegin=std::chrono::steady_clock::now();
        for(size_t i=0;i<1000000;++i)sum+=Runtime::original.get(double(i%262144));
        const double native=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-nativeBegin).count();
        require(sum>0);
        entry.init();require(Runtime::router.OverflowLive()==0);
        // Native InitPool clears slots but does not rewind its allocation cursor.
        // Preserve that behavior on bank zero; overflow storage remains reusable.
        const double afterReset=entry.alloc(4);require(afterReset>=0);
        require(entry.get(afterReset)==4 && entry.memory(0,512)==0);
        std::cout<<"native routing PASS live="<<count<<" banks="<<Runtime::router.BankCount()
                 <<" million_reads_ms="<<routed<<" original_ms="<<native<<"\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
