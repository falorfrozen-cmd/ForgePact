// Behavioral regression harness for gambapity's counter file (ForgePact #134
// phase 6): the load, the save and the version-2 migration, on a real disk.
//
// The Python runner injects the REAL GambaPityPath, GambaPitySave and
// GambaPityLoad from plugin/ModuleMain.cpp below, after the real decision core
// (GambaPity.hpp). Only what they reach outside that span is stubbed: the
// ItemTruth root (a scratch directory instead of %LOCALAPPDATA%), Out (a
// recorder) and the globals the adapter declares beside them.
//
// Live 1 (2026-10-06) found the migration held the old file open while the
// save renamed the new one over it; Windows refused the rename, the file
// stayed `{"count":12}` and a `.tmp` holding `{"version":2,"count":0}` was
// left beside it. A plain "could not save" also stayed on the status line
// after later saves landed. Each scenario below fails on that code.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

// PRODUCTION_GAMBAPITY

namespace ForgePact::ItemTruth {
static std::filesystem::path g_root;   // the harness's stand-in for ...\Hero_Siege\itemtruth
inline std::filesystem::path Root() { return g_root; }
}

static std::vector<std::string> g_out;
static void Out(const std::string& s) { g_out.push_back(s); }

static ForgePact::GambaPity::Pity g_GambaPity;
static bool g_GambaPityLoaded = false;
static std::string g_GambaPityError;
static bool g_GambaPityVersionError = false;
static bool g_GambaPitySaveError = false;

// PRODUCTION_GAMBAPITY_FILE

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::filesystem::path g_dir;

static std::string ReadText(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static void WriteText(const std::filesystem::path& p, const std::string& text)
{
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text;
}

// A fresh scratch directory and a fresh adapter state: nothing loaded, no
// error, a count of 0.
static void Reset(const std::string& name)
{
    g_dir = std::filesystem::temp_directory_path() / ("forgepact-gambapity-file-" + name);
    std::error_code ec;
    std::filesystem::remove_all(g_dir, ec);
    std::filesystem::create_directories(g_dir / "itemtruth");
    ForgePact::ItemTruth::g_root = g_dir / "itemtruth";
    g_out.clear();
    g_GambaPity = ForgePact::GambaPity::Pity();
    g_GambaPityLoaded = false;
    g_GambaPityError.clear();
    g_GambaPityVersionError = false;
    g_GambaPitySaveError = false;
}

static std::filesystem::path File() { return g_dir / "forgepact_gamba_pity.json"; }
static std::filesystem::path Tmp() { return g_dir / "forgepact_gamba_pity.json.tmp"; }

int main()
{
    namespace GP = ForgePact::GambaPity;

    // Baseline: a version-2 file loads its count, prints nothing and is not
    // rewritten.
    Reset("v2");
    WriteText(File(), GP::CounterFileText(7));
    GambaPityLoad();
    check("baseline/v2_loads_its_count", g_GambaPity.Count() == 7, std::to_string(g_GambaPity.Count()));
    check("baseline/v2_is_silent", g_out.empty() && g_GambaPityError.empty(), g_GambaPityError);
    check("baseline/v2_leaves_no_tmp", !std::filesystem::exists(Tmp()));

    // Target (Live 1's migrate-once): an older spin file is rewritten as
    // version 2 on the load that reads it, with no .tmp left beside it.
    Reset("legacy");
    WriteText(File(), "{\"count\":12}");
    GambaPityLoad();
    check("target/legacy_reads_as_zero", g_GambaPity.Count() == 0, std::to_string(g_GambaPity.Count()));
    check("target/legacy_prints_the_migration_line_once",
          g_out.size() == 1 && g_out[0] == GP::Pity::MigrationLine(), std::to_string(g_out.size()));
    check("target/legacy_file_is_rewritten_as_v2", ReadText(File()) == GP::CounterFileText(0), ReadText(File()));
    check("target/legacy_leaves_no_tmp", !std::filesystem::exists(Tmp()));
    check("target/legacy_leaves_no_error", g_GambaPityError.empty(), g_GambaPityError);
    // The next launch reads version 2 and is silent.
    g_GambaPityLoaded = false;
    g_out.clear();
    GambaPityLoad();
    check("target/legacy_next_load_is_silent", g_out.empty() && g_GambaPityError.empty(), g_GambaPityError);

    // A save that cannot land names itself and leaves no .tmp; the next save
    // that lands clears it from the status line.
    Reset("save-error");
    std::filesystem::create_directories(File());   // a directory where the file goes: the rename is refused
    g_GambaPity.SetCount(3);
    GambaPitySave();
    check("target/failed_save_names_itself", g_GambaPityError.find("could not save") == 0, g_GambaPityError);
    check("target/failed_save_leaves_no_tmp", !std::filesystem::exists(Tmp()));
    std::filesystem::remove_all(File());
    GambaPitySave();
    check("target/landed_save_clears_the_save_error", g_GambaPityError.empty(), g_GambaPityError);
    check("target/landed_save_writes_the_count", ReadText(File()) == GP::CounterFileText(3), ReadText(File()));

    // Another version: the status line's error, the file left as it is until a
    // save lands, which clears the error.
    Reset("unknown");
    WriteText(File(), "{\"version\":9,\"count\":4}");
    GambaPityLoad();
    check("target/unknown_version_shows_its_error",
          g_GambaPityError == GP::Pity::VersionErrorText(9) && g_GambaPityVersionError, g_GambaPityError);
    check("target/unknown_version_is_left_alone", ReadText(File()) == "{\"version\":9,\"count\":4}", ReadText(File()));
    g_GambaPity.SetCount(1);
    GambaPitySave();
    check("target/landed_save_clears_the_version_error", g_GambaPityError.empty() && !g_GambaPityVersionError, g_GambaPityError);
    check("target/landed_save_replaces_the_unknown_file", ReadText(File()) == GP::CounterFileText(1), ReadText(File()));

    // Negative control: a read error is not a save error, so a landed save
    // leaves it on the status line.
    Reset("read-error");
    g_GambaPityError = "could not read the gambapity counter";
    GambaPitySave();
    check("control/landed_save_keeps_a_read_error", g_GambaPityError == "could not read the gambapity counter", g_GambaPityError);

    std::error_code ec;
    std::filesystem::remove_all(g_dir, ec);
    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << std::endl;
    return failures ? 1 : 0;
}
