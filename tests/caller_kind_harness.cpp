// The enemy-born guard's caller reads (CallerObjectIndex, InstanceIdOf), compiled
// from ModuleMain.cpp and run against a stub runtime (issue #44, Live 1 rerun).
//
// The stub's ToDouble() stands in for the runner's REAL_RValue, which is what
// YYToolkit's RValue::ToDouble() calls. On a kind it cannot convert, the real
// routine raises the runner's own error ("REAL argument incorrect type
// undefined" in YYToolkit full report #2) and returns; it does not throw, so a
// `catch (...)` around the conversion never sees it. The stub does the same:
// it records the error and returns 0. Undefined is the measured rejection; the
// other kinds the stub rejects are the ones that carry no number.
#include <cstdio>
#include <initializer_list>
#include <string>
enum RValueType {
    VALUE_REAL = 0, VALUE_STRING = 1, VALUE_PTR = 3, VALUE_UNDEFINED = 5, VALUE_OBJECT = 6,
    VALUE_INT32 = 7, VALUE_INT64 = 10, VALUE_BOOL = 13, VALUE_REF = 15
};
static int g_RunnerErrors = 0;
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED; double value = 0; CInstance* inst = nullptr; const char* name = nullptr;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), value(n) {}
    RValue(CInstance* i) : m_Kind(VALUE_REF), inst(i) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), name(s) {}
    static RValue Of(int kind, double n) { RValue r; r.m_Kind = kind; r.value = n; return r; }
    double ToDouble() const {
        switch (m_Kind) {
        case VALUE_REAL: case VALUE_INT32: case VALUE_INT64: case VALUE_BOOL: case VALUE_REF: return value;
        default: ++g_RunnerErrors; return 0.0;
        }
    }
};
// What variable_instance_get answers for this instance's `object_index` and `id`.
struct CInstance { RValue objectIndex, id; RValue ToRValue() { return RValue(this); } };
struct YytkStub {
    RValue CallBuiltin(const char* fn, std::initializer_list<RValue> args) {
        if (std::string(fn) != "variable_instance_get" || args.size() != 2) return RValue();
        const RValue& self = *args.begin();
        const RValue& key = *(args.begin() + 1);
        if (!self.inst || !key.name) return RValue();
        if (std::string(key.name) == "object_index") return self.inst->objectIndex;
        if (std::string(key.name) == "id") return self.inst->id;
        return RValue();
    }
};
static YytkStub g_YytkStub;
static YytkStub* g_Yytk = &g_YytkStub;

// PRODUCTION_KIND_CHECK
// PRODUCTION_CALLER_OBJECT_INDEX
// PRODUCTION_INSTANCE_ID_OF

// The shape the guard had before the fix, kept here as the negative control: a
// harness whose stub never records an error would pass the targets for nothing.
static int UncheckedCallerObjectIndex(CInstance* S)
{
    if (!S) return -1;
    try { RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { RValue(S), RValue("object_index") }); return (int)oi.ToDouble(); } catch (...) { return -1; }
}

static bool g_AllPassed = true;
static void Report(const char* name, bool ok, const std::string& detail)
{
    std::printf("%s %s%s%s\n", name, ok ? "PASS" : "FAIL", detail.empty() ? "" : " ", detail.c_str());
    if (!ok) g_AllPassed = false;
}
static std::string Errors(int before) { return "errors=" + std::to_string(g_RunnerErrors - before); }

int main()
{
    {   // Baseline: every numeric kind the runtime produces for these reads comes back unchanged.
        const int before = g_RunnerErrors;
        CInstance real{ RValue::Of(VALUE_REAL, 71), RValue::Of(VALUE_REAL, 100001) };
        CInstance i32{ RValue::Of(VALUE_INT32, 74), RValue::Of(VALUE_INT32, 100004) };
        CInstance i64{ RValue::Of(VALUE_INT64, 72), RValue::Of(VALUE_INT64, 100002) };
        CInstance ref{ RValue::Of(VALUE_REF, 73), RValue::Of(VALUE_REF, 100003) };
        const bool ok = CallerObjectIndex(&real) == 71 && CallerObjectIndex(&i32) == 74
            && CallerObjectIndex(&i64) == 72 && CallerObjectIndex(&ref) == 73
            && InstanceIdOf(real.ToRValue()) == 100001.0 && InstanceIdOf(i32.ToRValue()) == 100004.0
            && InstanceIdOf(i64.ToRValue()) == 100002.0 && InstanceIdOf(ref.ToRValue()) == 100003.0
            && CallerObjectIndex(nullptr) == -1 && g_RunnerErrors == before;
        Report("numeric_reads", ok, Errors(before));
    }
    {   // Target: a `self` with no object_index (the `cb` spawn) reads as unknown, with no runner error.
        const int before = g_RunnerErrors;
        CInstance undef{ RValue(), RValue::Of(VALUE_REAL, 5) };
        CInstance text{ RValue("Karp_King_obj"), RValue::Of(VALUE_REAL, 6) };
        CInstance object{ RValue::Of(VALUE_OBJECT, 0), RValue::Of(VALUE_REAL, 7) };
        const int a = CallerObjectIndex(&undef), b = CallerObjectIndex(&text), c = CallerObjectIndex(&object);
        Report("undefined_caller_object_index", a == -1 && b == -1 && c == -1 && g_RunnerErrors == before,
               Errors(before) + " reads=" + std::to_string(a) + "," + std::to_string(b) + "," + std::to_string(c));
    }
    {   // Target: the same for the `id` read.
        const int before = g_RunnerErrors;
        CInstance undef{ RValue::Of(VALUE_REAL, 71), RValue() };
        CInstance text{ RValue::Of(VALUE_REAL, 71), RValue("100001") };
        const double a = InstanceIdOf(undef.ToRValue()), b = InstanceIdOf(text.ToRValue());
        Report("undefined_instance_id", a == -1.0 && b == -1.0 && g_RunnerErrors == before,
               Errors(before) + " reads=" + std::to_string(a) + "," + std::to_string(b));
    }
    {   // Negative control: the unchecked conversion, in this same harness, records the error.
        const int before = g_RunnerErrors;
        CInstance undef{ RValue(), RValue::Of(VALUE_REAL, 5) };
        UncheckedCallerObjectIndex(&undef);
        Report("unchecked_conversion_seen", g_RunnerErrors == before + 1, Errors(before));
    }
    std::printf("caller kind harness DONE %s\n", g_AllPassed ? "all-pass" : "some-fail");
    return 0;
}
