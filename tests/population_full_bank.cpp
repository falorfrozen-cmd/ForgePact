// A full native pool is not asked again until a free has given it room
// (ForgePact#232, live 2026-10-20). The fake pools model the supported
// library's measured allocator: a first-fit scan forward from a cursor, the
// cursor left where it was when the scan reaches the end and fails, and a free
// that moves the cursor back to the freed record. So a full pool whose cursor a
// free rewound scans every record past it before refusing. Live, with the
// game's pool and two extra pools full, that turned each new record into up to
// three whole-pool scans: 4-12 s freezes, 36 of 40 stall samples inside the
// full pools' scan loops.
#include <ForgePact/ProtectedPoolRouter.hpp>
#include <array>
#include <cstdio>
#include <stdexcept>

using namespace ForgePact::ProtectedPool;
static void check(bool b, const char* what) { if (!b) { std::printf("FAIL %s\n", what); throw std::runtime_error(what); } }

constexpr size_t C = 64;
template<int N> struct Pool {
    static inline std::array<bool, C> live{};
    static inline std::array<double, C> values{};
    static inline size_t cursor = 0;
    static inline size_t scanned = 0;   // records the allocator stepped over
    static double init() { return 0; }  // like the native one: the cursor stays
    static double alloc(double v) {
        for (size_t i = cursor; i < C; ++i) {
            ++scanned;
            if (!live[i]) { live[i] = true; values[i] = v; cursor = i + 1; return double(i); }
        }
        return -1;
    }
    static double get(double h) { return values[size_t(h)]; }
    static double set(double h, double v) { values[size_t(h)] = v; return 0; }
    static double free(double h) {
        const size_t i = size_t(h);
        if (live[i]) { live[i] = false; if (i < cursor) cursor = i; }
        return 0;
    }
    static double undef(double) { return 0; }
    static double scramble(double) { return 0; }
    static double protect(double) { return 0; }
    static double verify(double) { return 0; }
    static double memory(double, double) { return 0; }
    static Api api() { return {init, alloc, get, set, free, undef, scramble, protect, verify, memory}; }
};

static int Run();
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try { return Run(); } catch (const std::exception&) { return 1; }
}

static int Run() {
    // The game's own pool already holds records when the router attaches.
    for (size_t i = 0; i < C / 2; ++i) Pool<0>::alloc(1);
    Router<C, 3> r;
    int grown = 0;
    r.Attach(Pool<0>::api(), [&](Api& a) { a = grown++ == 0 ? Pool<1>::api() : Pool<2>::api(); return true; }, -77);

    // Fill bank zero and bank one; the rest go to bank two.
    for (size_t i = 0; i < C / 2 + C + 8; ++i) check(r.Allocate(2) >= 0, "allocation succeeds");
    check(r.OverflowLive() == C + 8, "bank one full, eight in bank two");

    // Baseline the vanilla game would reach: a free in the game's pool rewinds
    // its cursor to the start of the table.
    r.Free(0);
    check(r.Allocate(3) == 0, "the freed record in the game's pool is reused first");
    // A free in full bank one rewinds its cursor to its first record.
    r.Free(double(C));
    check(r.Allocate(4) == double(C), "the freed record in bank one is reused next");

    // Target: both pools are full again. Further records must go to bank two
    // without scanning either full pool.
    const size_t before0 = Pool<0>::scanned, before1 = Pool<1>::scanned;
    for (int i = 0; i < 40; ++i) check(r.Allocate(5) >= double(2 * C), "lands in bank two");
    std::printf("full-pool scans after refusal: game=%zu bank1=%zu\n",
                Pool<0>::scanned - before0, Pool<1>::scanned - before1);
    check(Pool<0>::scanned - before0 <= C, "the game's full pool is not scanned on every allocation");
    check(Pool<1>::scanned == before1, "a counted-full extra pool is never asked");

    // A later free in the game's pool is still found: no record is stranded.
    r.Free(5);
    check(r.Allocate(6) == 5, "a free in the full game pool reopens it");
    check(r.Failures() == 0 && r.InvalidHandles() == 0, "no failures");

    // After a native reset the game's pool is probed again.
    r.Reset();
    std::printf("full bank: skipped until freed PASS\n");
    return 0;
}
