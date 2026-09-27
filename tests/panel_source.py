"""The control panel's frontend source, as text, for the contract tests.

The panel's markup, CSS and JavaScript used to live in one string in
`src/forgepact.py` (its `HTML` constant); they are now the Svelte + Vite project
under `panel/src/`. Tests that pin a fact about the page read it from here:

- `panel_source()`: every source file joined, `App.svelte` first, then
  `tabs/*.svelte`, then `lib/**`, then the remaining `*.js`/`*.css`/`*.svelte`
  (each group sorted by path), each preceded by a `/* == <relative path> == */`
  line, so a search across "the page" still has one string to search;
- `panel_file(rel)`: one file, when a fact belongs to a specific file;
- `js_for_node(text)`: a sliced function made runnable under plain `node`
  again, by dropping the `export ` prefixes and `import ...;` lines the
  modules carry.

`FORGEPACT_TEST_PANEL_SRC` points at another copy of `panel/src` (the
"prove the instrument" precedent: run the same tests against an older tree).
`FORGEPACT_TEST_PANEL_DIR` keeps meaning the directory holding
`forgepact.py`, for the Python-side facts.

Stdlib only, and it imports nothing that imports `forgepact`, so the tests
that must run without `hs_game_sdk` on the path can use it.
"""
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PANEL_SRC = Path(os.environ.get("FORGEPACT_TEST_PANEL_SRC", str(ROOT / "panel" / "src")))

_SOURCE_SUFFIXES = (".js", ".css", ".svelte")


def _rel(path):
    return path.relative_to(PANEL_SRC).as_posix()


def panel_files():
    """The panel's source files, in the order panel_source() joins them."""
    app = PANEL_SRC / "App.svelte"
    tabs = sorted((PANEL_SRC / "tabs").glob("*.svelte"), key=_rel)
    lib_dir = PANEL_SRC / "lib"
    lib = sorted((p for p in lib_dir.rglob("*") if p.is_file()), key=_rel) if lib_dir.is_dir() else []
    taken = {app, *tabs, *lib}
    rest = sorted((p for p in PANEL_SRC.iterdir()
                   if p.is_file() and p.suffix in _SOURCE_SUFFIXES and p not in taken), key=_rel)
    return [p for p in [app, *tabs, *lib, *rest] if p.is_file()]


def panel_file(rel):
    """One source file's text, by its path relative to panel/src."""
    return (PANEL_SRC / rel).read_text(encoding="utf-8")


def panel_source():
    """Every panel source file, joined with a `/* == <path> == */` line before each."""
    return "".join(f"/* == {_rel(p)} == */\n" + p.read_text(encoding="utf-8") + "\n"
                   for p in panel_files())


_IMPORT_RE = re.compile(r"^[ \t]*import\b[^;]*;[ \t]*\n?", re.M)
_EXPORT_RE = re.compile(r"(^|[\s;{}])export (?=(?:async\s+)?function\b|const\b|let\b|var\b|class\b)", re.M)


def js_for_node(text):
    """`text` without `import ...;` lines or `export ` prefixes, runnable by node."""
    return _EXPORT_RE.sub(r"\1", _IMPORT_RE.sub("", text))
