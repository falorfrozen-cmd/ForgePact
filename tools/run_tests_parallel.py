"""Run ForgePact's Python suite in parallel, one worker process per test module.

    py -3 tools/run_tests_parallel.py            # from ForgePact/, all cores
    py -3 tools/run_tests_parallel.py -j 4 -v    # four workers, a line per module
    py -3 ForgePact/tools/run_tests_parallel.py  # from the hub root: the same run

It runs exactly what `py -3 -m unittest discover -s tests` runs. The parent
discovers the suite the same way, groups the test ids by module, and hands
each module to its own `python run_tests_parallel.py --worker` process, so a
module's class and module fixtures, its patches and its imports behave as they
do serially. After the run it refuses (exit 2) unless the ids the workers
loaded are exactly the ids serial discovery found, each loaded once.

The module is the unit of isolation. Two modules may run at the same time, so
a module must not share a fixed path, port or cwd with another one; a native
harness writes into its own `build/<name>` directory for that reason.

Modules are started longest first, from durations recorded in
`build/test-durations.json` by the previous run. A module that sets
`PARALLEL_GROUP = "<name>"` at top level shares a concurrency cap with the
other modules in that group (`--group-limit <name>=<n>`, default 1), for
suites that drive something that does not tolerate many copies, such as a
headless browser.

Standard library only.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / "build"  # test-durations.json, and test-logs/ for failing modules
GROUP_RE = re.compile(r"""^PARALLEL_GROUP\s*=\s*["']([\w-]+)["']""", re.M)


def iter_tests(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from iter_tests(item)
        else:
            yield item


def discover(start_dir):
    """The ids serial discovery runs, as the unittest CLI would find them."""
    start = str(Path(start_dir).resolve())
    suite = unittest.TestLoader().discover(start)
    return [test.id() for test in iter_tests(suite)]


def module_of(test_id):
    # A module that fails to import is reported as
    # `unittest.loader._FailedTest.<module>`; keep it with its module so the
    # worker reproduces the same import error.
    if test_id.startswith("unittest.loader._FailedTest."):
        return test_id.rsplit(".", 1)[1]
    return test_id.split(".", 1)[0]


def shard(ids):
    modules = {}
    for test_id in ids:
        modules.setdefault(module_of(test_id), []).append(test_id)
    return modules


def load_durations(state):
    try:
        return json.loads((state / "test-durations.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def save_durations(state, durations):
    try:
        state.mkdir(parents=True, exist_ok=True)
        tmp = state / f"test-durations.{os.getpid()}.tmp"
        tmp.write_text(json.dumps(durations, indent=1, sort_keys=True), encoding="utf-8")
        os.replace(tmp, state / "test-durations.json")
    except OSError:
        pass  # a cache: losing it only costs balance on the next run


def keep_log(state, name, output):
    """A failing module's full worker output, for what its traceback omits."""
    try:
        (state / "test-logs").mkdir(parents=True, exist_ok=True)
        (state / "test-logs" / f"{name}.log").write_text(output, encoding="utf-8")
    except OSError:
        pass


def order(modules, start_dir, durations):
    """Longest first; a module with no recorded time goes first by file size."""
    def key(name):
        path = Path(start_dir) / f"{name}.py"
        size = path.stat().st_size if path.exists() else 0
        known = durations.get(name)
        return (known is not None, -(known or 0), -size)
    return sorted(modules, key=key)


def group_of(name, start_dir):
    try:
        text = (Path(start_dir) / f"{name}.py").read_text(encoding="utf-8")
    except OSError:
        return None
    match = GROUP_RE.search(text)
    return match.group(1) if match else None


# ---------------------------------------------------------------- worker side

def load_module(start_dir, module):
    """One module's suite, loaded the way serial discovery loads it."""
    # Discovery restricted to one file: same import, same load_tests protocol.
    return unittest.TestLoader().discover(str(Path(start_dir).resolve()),
                                          pattern=f"{module}.py")


def run_worker(start_dir, module, out_path):
    suite = load_module(start_dir, module)
    loaded = [test.id() for test in iter_tests(suite)]
    # Written before running, so a worker that dies mid-module still accounts
    # for what it loaded and the parent reports a crash, not missing tests.
    Path(out_path + ".loaded").write_text(json.dumps(loaded), encoding="utf-8")
    runner = unittest.TextTestRunner(stream=sys.stderr, verbosity=2)
    result = runner.run(suite)
    describe = lambda pairs: [[str(test), text] for test, text in pairs]
    payload = {
        "module": module,
        "loaded": loaded,
        "testsRun": result.testsRun,
        "failures": describe(result.failures),
        "errors": describe(result.errors),
        "skipped": describe(result.skipped),
        "expectedFailures": describe(result.expectedFailures),
        "unexpectedSuccesses": [str(test) for test in result.unexpectedSuccesses],
    }
    Path(out_path).write_text(json.dumps(payload), encoding="utf-8")
    return 0


# ---------------------------------------------------------------- parent side

def run_parallel(start_dir, jobs, verbose, group_limits, state=STATE, stream=sys.stderr):
    """Run every module; returns (merged result dict, discovered ids)."""
    ids = discover(start_dir)
    durations = load_durations(state)
    pending = order(shard(ids), start_dir, durations)
    groups = {name: group_of(name, start_dir) for name in pending}
    running, results = {}, []
    began = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="forgepact-tests-") as work:
        work = Path(work)

        def launch(name):
            out = work / f"{name}.json"
            log = open(work / f"{name}.log", "w+", encoding="utf-8", errors="replace")
            proc = subprocess.Popen(
                [sys.executable, str(Path(__file__).resolve()), "--worker", name,
                 "--start-dir", str(Path(start_dir).resolve()), "--out", str(out)],
                cwd=str(Path.cwd()), stdout=log, stderr=subprocess.STDOUT,
                # A console run writes UTF-8; a redirected one would not.
                env={**os.environ, "PYTHONIOENCODING": "utf-8"})
            running[name] = (proc, out, log, time.perf_counter())

        def collect(name):
            proc, out, log, started = running.pop(name)
            log.seek(0)
            output = log.read()
            log.close()
            try:
                payload = json.loads(out.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                # The worker died before reporting: one error for the module,
                # carrying whatever it printed.
                try:
                    loaded = json.loads(Path(f"{out}.loaded").read_text(encoding="utf-8"))
                except (OSError, ValueError):
                    loaded = []
                payload = {"module": name, "loaded": loaded, "testsRun": 0, "failures": [],
                           "skipped": [], "expectedFailures": [], "unexpectedSuccesses": [],
                           "errors": [[f"{name} (worker exit {proc.returncode})",
                                       output[-4000:]]]}
            payload["wall"] = time.perf_counter() - started
            results.append(payload)
            durations[name] = round(payload["wall"], 2)
            bad = payload["failures"] or payload["errors"] or payload["unexpectedSuccesses"]
            if bad:
                keep_log(state, name, output)
            if verbose:
                print(f"{'FAIL' if bad else 'ok  '} {name} "
                      f"({payload['testsRun']} tests, {payload['wall']:.1f}s)",
                      file=stream, flush=True)
            else:
                stream.write("F" if bad else ".")
                stream.flush()

        try:
            while pending or running:
                for name in list(pending):
                    if len(running) >= jobs:
                        break
                    group = groups[name]
                    if group and sum(groups[n] == group for n in running) >= group_limits.get(group, 1):
                        continue
                    pending.remove(name)
                    launch(name)
                time.sleep(0.05)
                for name in [n for n, entry in running.items() if entry[0].poll() is not None]:
                    collect(name)
        finally:
            # Anything still running here was interrupted; end only what we started.
            for proc, _out, log, _started in running.values():
                proc.kill()
                proc.wait()
                log.close()
    if not verbose:
        stream.write("\n")
    save_durations(state, durations)
    merged = merge(results)
    merged["wall"] = time.perf_counter() - began
    return merged, ids


def merge(results):
    merged = {"loaded": [], "testsRun": 0, "failures": [], "errors": [], "skipped": [],
              "expectedFailures": [], "unexpectedSuccesses": []}
    for payload in sorted(results, key=lambda p: p["module"]):
        merged["loaded"].extend(payload["loaded"])
        merged["testsRun"] += payload["testsRun"]
        for key in ("failures", "errors", "skipped", "expectedFailures", "unexpectedSuccesses"):
            merged[key].extend(payload[key])
    return merged


def coverage_problems(discovered, loaded):
    """Why the loaded ids are not exactly the discovered ids, if they are not."""
    want, got = Counter(discovered), Counter(loaded)
    problems = []
    missing = sorted((want - got).elements())
    extra = sorted((got - want).elements())
    if missing:
        problems.append(f"{len(missing)} discovered test(s) never loaded, e.g. {missing[:3]}")
    if extra:
        problems.append(f"{len(extra)} loaded test(s) serial discovery does not run, e.g. {extra[:3]}")
    return problems


def summary(merged):
    """unittest's closing lines: 'Ran N tests in Xs' and 'OK'/'FAILED (...)'."""
    parts = []
    failed = merged["failures"] or merged["errors"] or merged["unexpectedSuccesses"]
    for label, key in (("failures", "failures"), ("errors", "errors")):
        if merged[key]:
            parts.append(f"{label}={len(merged[key])}")
    for label, key in (("skipped", "skipped"), ("expected failures", "expectedFailures"),
                       ("unexpected successes", "unexpectedSuccesses")):
        if merged[key]:
            parts.append(f"{label}={len(merged[key])}")
    status = "FAILED" if failed else "OK"
    if parts:
        status += f" ({', '.join(parts)})"
    return f"Ran {merged['testsRun']} tests in {merged['wall']:.3f}s", status


def report(merged, stream):
    line = "=" * 70
    for flavour, key in (("ERROR", "errors"), ("FAIL", "failures")):
        for desc, text in merged[key]:
            print(line, file=stream)
            print(f"{flavour}: {desc}", file=stream)
            print("-" * 70, file=stream)
            print(text, file=stream)
    for desc in merged["unexpectedSuccesses"]:
        print(line, file=stream)
        print(f"UNEXPECTED SUCCESS: {desc}", file=stream)
    print("-" * 70, file=stream)
    ran, status = summary(merged)
    print(ran, file=stream)
    print(file=stream)
    print(status, file=stream)


def parse_limits(values):
    limits = {}
    for value in values:
        name, _, count = value.partition("=")
        limits[name] = max(1, int(count))
    return limits


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 1,
                        help="worker processes (default: os.cpu_count())")
    parser.add_argument("-s", "--start-dir",
                        help="default: ForgePact's tests/, run from ForgePact/ wherever "
                             "this is invoked; a given directory is relative to the cwd, "
                             "as for unittest")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="a line per module instead of a dot")
    parser.add_argument("--group-limit", action="append", default=[], metavar="GROUP=N",
                        help="cap for modules declaring PARALLEL_GROUP (default 1 each)")
    parser.add_argument("--state-dir", type=Path, default=STATE,
                        help="where test-durations.json and failing modules' logs go "
                             "(default: build/)")
    parser.add_argument("--list", action="store_true", help="print the discovered ids and exit")
    parser.add_argument("--worker", metavar="MODULE", help=argparse.SUPPRESS)
    parser.add_argument("--out", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    if args.start_dir is None:
        # The serial reference runs from ForgePact/, and some tests read paths
        # relative to it; so does this, even when invoked from the hub root.
        os.chdir(ROOT)
        args.start_dir = "tests"
    # Run as a script, sys.path[0] is tools/; `python -m unittest` has the cwd
    # there instead. Match it, so no test imports here what it could not there.
    if sys.path and Path(sys.path[0]).resolve() == Path(__file__).resolve().parent:
        sys.path[0] = os.getcwd()

    if args.worker:
        return run_worker(args.start_dir, args.worker, args.out)
    if args.list:
        for test_id in discover(args.start_dir):
            print(test_id)
        return 0

    merged, discovered = run_parallel(args.start_dir, max(1, args.jobs), args.verbose,
                                      parse_limits(args.group_limit), args.state_dir)
    report(merged, sys.stderr)
    problems = coverage_problems(discovered, merged["loaded"])
    for problem in problems:
        print(f"run_tests_parallel: {problem}", file=sys.stderr)
    if problems:
        return 2
    failed = merged["failures"] or merged["errors"] or merged["unexpectedSuccesses"]
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
