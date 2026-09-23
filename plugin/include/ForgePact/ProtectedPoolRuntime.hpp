#pragma once
#include "ProtectedPoolRouter.hpp"
#include "PopulationProfile.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <string>
#include <vector>
#include <deque>
#include <type_traits>
#pragma comment(lib,"bcrypt.lib")

namespace ForgePact::ProtectedPool::Runtime {
inline constexpr char kSupportedSha256[]="ea33261a54ba922b4074ae211990087c3a74fa840ead44ab30f0c74e504c505a";
inline constexpr size_t kCapacity=262144,kReserve=16384;
inline Api original;
inline Router<> router;
inline std::atomic<bool> active{false};
inline bool attempted=false;
inline std::string reason="Not requested";
inline std::filesystem::path source,cache;
inline std::vector<HMODULE> modules; // Remain loaded while any game handle can reference them.
inline std::deque<Api> prepared;
inline std::function<void(const std::string&)> log;
inline uint64_t nextReserveAttempt=0;
using Installer=std::function<bool(const char*,void*,void*,void**)>;

inline std::string Hash(const std::filesystem::path& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    std::array<unsigned char,32> digest{};std::array<unsigned char,32768> buffer{};
    bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok)ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0;
    while(ok) {
        DWORD got=0;if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&got,nullptr)){ok=false;break;}
        if(!got)break;
        ok=BCryptHashData(hash,buffer.data(),got,0)>=0;
    }
    if(ok)ok=BCryptFinishHash(hash,digest.data(),DWORD(digest.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);CloseHandle(file);
    if(!ok)return {};
    const char* hex="0123456789abcdef";std::string result;
    for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
inline void* Export(HMODULE module,const char* name) {
    auto fn=reinterpret_cast<void*>(GetProcAddress(module,name));MEMORY_BASIC_INFORMATION info{};
    if(!fn || VirtualQuery(fn,&info,sizeof(info))!=sizeof(info) || info.AllocationBase!=module
       || info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return nullptr;
    const DWORD protection=info.Protect&0xff;
    if(protection!=PAGE_EXECUTE && protection!=PAGE_EXECUTE_READ && protection!=PAGE_EXECUTE_READWRITE && protection!=PAGE_EXECUTE_WRITECOPY)return nullptr;
    return fn;
}
inline Api Resolve(HMODULE m) {
    return {reinterpret_cast<Fn0>(Export(m,"InitPool")),reinterpret_cast<Fn1>(Export(m,"InitNewVariableFast")),
        reinterpret_cast<Fn1>(Export(m,"GetVariable")),reinterpret_cast<Fn2>(Export(m,"SetVariable")),
        reinterpret_cast<Fn1>(Export(m,"FreeVariable")),reinterpret_cast<Fn1>(Export(m,"SetVariableToUndefined")),
        reinterpret_cast<Fn1>(Export(m,"ScrambleKey")),reinterpret_cast<Fn1>(Export(m,"ProtectVariable")),
        reinterpret_cast<Fn1>(Export(m,"TamperCheck")),reinterpret_cast<Fn2>(Export(m,"MemoryTamperCheck"))};
}
inline bool LoadBank(Api& api) {
    if(!prepared.empty()){api=prepared.front();prepared.pop_front();return true;}
    try {
        const auto path=cache/(L"ForgePactPool-"+std::to_wstring(modules.size())+L".dll");
        std::filesystem::create_directories(cache);
        if(!std::filesystem::exists(path))std::filesystem::copy_file(source,path);
        if(Hash(path)!=kSupportedSha256){reason="Capacity cache identity mismatch";return false;}
        // Keep a read-only file handle across loading to prevent a hash/load race.
        HANDLE hold=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(hold==INVALID_HANDLE_VALUE){reason="Capacity cache is unavailable";return false;}
        const bool verified=Hash(path)==kSupportedSha256;
        HMODULE mod=verified?LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32):nullptr;
        CloseHandle(hold);
        if(!mod){reason="Additional native pool could not be loaded";return false;}
        for(auto existing:modules)if(existing==mod){FreeLibrary(mod);reason="Native pool was not independent";return false;}
        api=Resolve(mod);
        if(!api.Complete()){FreeLibrary(mod);reason="Additional pool exports unavailable";return false;}
        if(api.init()!=0){FreeLibrary(mod);reason="Additional pool initialization failed";return false;}
        modules.push_back(mod);
        return true;
    }catch(...){reason="Additional pool storage is unavailable";return false;}
}

inline double Init(){return active.load(std::memory_order_acquire)?router.Reset():original.init();}
inline double Allocate(double v){FP_POP_SCOPE(PoolAllocate);return active.load(std::memory_order_acquire)?router.Allocate(v):original.alloc(v);}
inline double Get(double h){return active.load(std::memory_order_acquire)?router.Get(h):original.get(h);}
inline double Set(double h,double v){FP_POP_SCOPE(PoolSet);return active.load(std::memory_order_acquire)?router.Set(h,v):original.set(h,v);}
inline double Free(double h){FP_POP_SCOPE(PoolFree);return active.load(std::memory_order_acquire)?router.Free(h):original.free(h);}
inline double Undefine(double h){return active.load(std::memory_order_acquire)?router.Undefine(h):original.undef(h);}
inline double Scramble(double h){return active.load(std::memory_order_acquire)?router.Scramble(h):original.scramble(h);}
inline double Protect(double h){return active.load(std::memory_order_acquire)?router.Protect(h):original.protect(h);}
inline double Check(double h){return active.load(std::memory_order_acquire)?router.Check(h):original.check(h);}
inline double CheckMemory(double b,double n){FP_POP_SCOPE(PoolMemoryCheck);return active.load(std::memory_order_acquire)?router.CheckMemory(b,n):NativeMemoryCheck(original.memory,b,n);}

inline bool Install(HMODULE module,const std::filesystem::path& cacheRoot,Installer install,
                    std::function<void(const std::string&)> logger) {
    if(attempted)return active.load();log=std::move(logger);
    auto refuse=[](const char* why){reason=why;if(log)log("population: "+reason);return false;};
    if(!module){reason="Waiting for native pool library";return false;}
    attempted=true;
    std::wstring path(32768,L'\0');DWORD size=GetModuleFileNameW(module,path.data(),DWORD(path.size()));
    if(!size || size>=path.size())return refuse("Native pool library path unavailable");
    path.resize(size);source=path;cache=cacheRoot/kSupportedSha256;
    if(Hash(source)!=kSupportedSha256)return refuse("Unsupported native pool library; early population unavailable");
    original=Resolve(module);
    if(!original.Complete())return refuse("Native pool exports unavailable");
    modules.push_back(module);
    Api spare;
    if(!LoadBank(spare))return refuse("Could not prepare capacity reserve");
    const double control=spare.alloc(0.25);
    if(control!=0 || spare.get(control)!=0.25 || spare.check(control)!=0)return refuse("Native pool control failed");
    spare.undef(control);const double undefinedValue=spare.get(control);spare.free(control);
    const double integrity=NativeMemoryCheck(spare.memory,0,512);
    if(!std::isfinite(undefinedValue) || integrity!=0) {
        if(log)log("population: reserve control undefined="+std::to_string(undefinedValue)+" integrity="+std::to_string(integrity));
        return refuse("Native pool integrity control failed");
    }
    prepared.push_back(spare);
    // Install every consumer BEFORE allocation can return extended handles.
    // A partial installation remains an inert forwarding layer for this process.
    auto hook=[&](const char* name,auto replacement,auto& target) {
        void* trampoline=nullptr;
        if(!install(name,reinterpret_cast<void*>(target),reinterpret_cast<void*>(replacement),&trampoline) || !trampoline)return false;
        target=reinterpret_cast<std::remove_reference_t<decltype(target)>>(trampoline);return true;
    };
    if(!hook("GetVariable",Get,original.get) || !hook("SetVariable",Set,original.set)
       || !hook("FreeVariable",Free,original.free) || !hook("SetVariableToUndefined",Undefine,original.undef)
       || !hook("ScrambleKey",Scramble,original.scramble) || !hook("ProtectVariable",Protect,original.protect)
       || !hook("TamperCheck",Check,original.check) || !hook("MemoryTamperCheck",CheckMemory,original.memory)
       || !hook("InitPool",Init,original.init) || !hook("InitNewVariableFast",Allocate,original.alloc))
        return refuse("Capacity hooks incomplete; early population unavailable");
    router.Attach(original,LoadBank,undefinedValue);
    if(!router.EnsureHeadroom(kCapacity*2))return refuse("Capacity reserve unavailable; early population unavailable");
    reason="Capacity reserve ready";active.store(true,std::memory_order_release);
    if(log)log("population: capacity routing ready ("+std::to_string(router.BankCount())+" native pools)");
    return true;
}
inline bool CanPopulate(){return active.load() && router.Headroom()>=kReserve && router.Failures()==0;}
inline void Maintain(uint64_t frame) {
    if(!active.load())return;
    if(router.Failures()) {
        const std::string stopped="Native allocation failed; early population stopped for this session";
        if(reason!=stopped){reason=stopped;if(log)log("population: "+reason);}return;
    }
    if(frame<nextReserveAttempt)return;
    if(router.Headroom()<kCapacity/2) {
        nextReserveAttempt=frame+300;
        const auto before=router.BankCount();
        if(!router.EnsureHeadroom(kCapacity)) {reason="Waiting for capacity reserve";if(log)log("population: "+reason);}
        else {reason="Capacity reserve ready";if(log && router.BankCount()!=before)log("population: reserve expanded to "+std::to_string(router.BankCount())+" native pools");}
    }
}
}
