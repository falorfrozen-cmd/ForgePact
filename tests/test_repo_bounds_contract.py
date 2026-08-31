import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"


class RepoBoundsContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")
        match = re.search(
            r"kSeason10RepoCounts\[\]\s*=\s*\{(?P<body>.*?)\};",
            cls.plugin,
            re.DOTALL,
        )
        if not match:
            raise AssertionError("Season 10 repository bounds table was not found")
        cls.counts = [int(value) for value in re.findall(r"\b\d+\b", match.group("body"))]

    def test_verified_season10_category_counts_are_exact(self):
        self.assertEqual(
            self.counts,
            [15, 20, 15, 0, 20, 25, 18, 30, 15, 0,
             60, 27, 44, 65, 74, 200, 156, 0, 16, 7],
        )

    def test_relic_and_socketable_boundaries_fail_closed(self):
        def valid(category: int, index: int) -> bool:
            return (
                0 <= category < len(self.counts)
                and 0 <= index < self.counts[category]
            )

        self.assertTrue(valid(16, 155))
        for index in (156, 157, 158, 159, 160, 161):
            self.assertFalse(valid(16, index), index)
        self.assertTrue(valid(15, 199))
        self.assertFalse(valid(15, 200))
        for category in (3, 9, 17, 20):
            self.assertFalse(valid(category, 0), category)

    def test_every_normal_repo_call_passes_the_central_guard(self):
        self.assertEqual(
            self.plugin.count('CallGameScript("gml_Script_GetNormalRepoStruct"'),
            1,
            "new direct calls must not bypass RepoStruct's bounds gate",
        )
        wrapper = re.search(
            r"static\s+bool\s+RepoStruct\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(wrapper)
        body = wrapper.group("body")
        guard = "if (!RepoIndexValid(kategori, indeks)) return false;"
        self.assertIn(guard, body)
        self.assertLess(body.index(guard), body.index("GetNormalRepoStruct"))

    def test_relic_scan_never_uses_invalid_sentinel_probes(self):
        branch = re.search(
            r'else\s+if\s*\(grup\s*==\s*"relic"\)\s*\{(?P<body>.*?)\n\s*return;',
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(branch)
        body = branch.group("body")
        self.assertIn("i < kSeason10RelicRepoCount", body)
        self.assertNotIn("bosSayac", body)


if __name__ == "__main__":
    unittest.main()
