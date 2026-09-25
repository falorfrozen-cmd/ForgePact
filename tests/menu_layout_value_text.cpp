// Native harness for menulayout's value formatter (tests/test_stash_bag_layout_contract.py).
// The test cuts the production formatter out of plugin/ModuleMain.cpp and
// pastes it where the marker below sits, so this file only supplies a stand-in
// runtime: an RValue that can be every kind the listing meets on this runner
// (arrays, nested arrays, and instance handles as VALUE_REF, not only reals and
// strings), and a CallBuiltin that answers array_length/array_get and can be
// told to throw, the way a failed read does in the game.
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

enum RValueKind { VALUE_REAL = 0, VALUE_STRING = 1, VALUE_ARRAY = 2, VALUE_PTR = 3, VALUE_UNDEFINED = 5,
                  VALUE_OBJECT = 6, VALUE_INT32 = 7, VALUE_INT64 = 10, VALUE_NULL = 12, VALUE_BOOL = 13,
                  VALUE_REF = 15 };

struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double real = 0.0;
    std::string str;
    std::shared_ptr<std::vector<RValue>> items;
    RValue() = default;
    RValue(double d) : m_Kind(VALUE_REAL), real(d) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), str(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), str(s) {}
    static RValue Bool(bool b) { RValue v; v.m_Kind = VALUE_BOOL; v.real = b ? 1.0 : 0.0; return v; }
    static RValue Ref(double id) { RValue v; v.m_Kind = VALUE_REF; v.real = id; return v; }
    static RValue OfKind(int kind) { RValue v; v.m_Kind = kind; return v; }
    static RValue Array(std::vector<RValue> e) {
        RValue v; v.m_Kind = VALUE_ARRAY; v.items = std::make_shared<std::vector<RValue>>(std::move(e)); return v;
    }
    double ToDouble() const { return real; }
    bool ToBoolean() const { return real != 0.0; }
    // What the runner's string conversion makes of anything but a string has
    // not been measured, so the stand-in refuses rather than inventing a
    // rendering a test could then bless: a formatter that reaches ToString
    // for another kind fails here.
    std::string ToString() const {
        if (m_Kind == VALUE_STRING) return str;
        throw std::logic_error("ToString on kind " + std::to_string(m_Kind));
    }
};

struct StubRuntime {
    int failGetAt = -1;          // array_get throws for this index
    bool failLength = false;     // array_length throws
    RValue CallBuiltin(const char* name, const std::vector<RValue>& args) {
        const std::string n = name;
        if (n == "array_length") {
            if (failLength) throw std::runtime_error("array_length");
            return RValue((double)args.at(0).items->size());
        }
        if (n == "array_get") {
            const int i = (int)args.at(1).ToDouble();
            if (i == failGetAt) throw std::runtime_error("array_get");
            return args.at(0).items->at((size_t)i);
        }
        throw std::runtime_error("unexpected builtin " + n);
    }
};
static StubRuntime g_Stub;
static StubRuntime* g_Yytk = &g_Stub;

// PRODUCTION_VALUE_TEXT

int main()
{
    int failed = 0;
    auto check = [&](const std::string& got, const std::string& want, const char* label) {
        const bool ok = got == want;
        std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << '\n';
        if (!ok) ++failed;
    };
    // Negative controls: a value that is not an array gets no brackets.
    check(MenuLayoutValueText(RValue(7.0)), "7", "scalar/whole real");
    check(MenuLayoutValueText(RValue(2.5)), "2.5", "scalar/fraction");
    check(MenuLayoutValueText(RValue("[1,2]")), "[1,2]", "scalar/string that looks like an array is printed as is");
    check(MenuLayoutValueText(RValue::Ref(257590)), "<ref>", "scalar/instance handle (VALUE_REF) is named by kind, not converted");
    check(MenuLayoutValueText(RValue::OfKind(VALUE_OBJECT)), "<object>", "scalar/struct or method is named by kind");
    check(MenuLayoutValueText(RValue::OfKind(VALUE_PTR)), "<ptr>", "scalar/pointer is named by kind");
    check(MenuLayoutValueText(RValue::OfKind(VALUE_NULL)), "<kind 12>", "scalar/any other kind prints its number");
    // Arrays.
    check(MenuLayoutValueText(RValue::Array({ RValue(1.0), RValue("Materials"), RValue::Bool(true), RValue(2.5) })),
          "[1,Materials,1,2.5]", "array/each element through the value formatter");
    check(MenuLayoutValueText(RValue::Array({})), "[]", "array/empty");
    check(MenuLayoutValueText(RValue::Array({ RValue(-4.0), RValue::Array({ RValue(1.0), RValue(2.0) }) })),
          "[-4,<array>]", "array/nested array prints <array>, never recursing");
    check(MenuLayoutValueText(RValue::Array({ RValue::Ref(12.0), RValue(), RValue::OfKind(VALUE_OBJECT) })),
          "[<ref>,undefined,<object>]", "array/handle, undefined and struct elements");
    {
        std::vector<RValue> many;
        for (int i = 0; i < 40; ++i) many.push_back(RValue((double)i));
        std::string want = "[";
        for (int i = 0; i < 32; ++i) want += (i ? "," : "") + std::to_string(i);
        check(MenuLayoutValueText(RValue::Array(many)), want + ",...+8]", "array/past 32 elements the rest are counted");
        many.resize(32);
        check(MenuLayoutValueText(RValue::Array(many)), want + "]", "array/exactly 32 elements print whole");
    }
    check(MenuLayoutValueText(RValue::Array({ RValue(std::string("a\nb")) })), "[a b]",
          "array/a line break in an element cannot start a new row");
    g_Stub.failGetAt = 1;
    check(MenuLayoutValueText(RValue::Array({ RValue(1.0), RValue(2.0), RValue(3.0) })), "[1,<read-failed>,3]",
          "array/one failed element read says so and the rest still print");
    g_Stub.failGetAt = -1;
    g_Stub.failLength = true;
    check(MenuLayoutValueText(RValue::Array({ RValue(1.0) })), "<read-failed>", "array/failed length read");
    g_Stub.failLength = false;
    std::cout << (failed ? "RESULT FAILED" : "RESULT OK") << '\n';
    return failed ? 1 : 0;
}
