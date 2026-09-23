"""Summarize the bounded local profile; inclusive timings must not be added."""
import argparse
import json
from pathlib import Path


def intervals(rows):
    # New captures explicitly identify their start before native generation.
    # Preserve work before the first Present row; do not invent CPU snapshots
    # or a room identity for that initial, possibly partial-frame interval.
    if rows and rows[0].get("captureStart") and rows[0]["elapsedUs"] > 0:
        baseline = dict(elapsedUs=0, frames=0, room=None, initialBaseline=True,
                        metrics={name: dict(calls=0, samples=0, sampledUs=0)
                                 for name in rows[0]["metrics"]})
        rows = [baseline, *rows]
    for before, after in zip(rows, rows[1:]):
        frames = after["frames"] - before["frames"]
        elapsed = after["elapsedUs"] - before["elapsedUs"]
        if frames <= 0 or elapsed <= 0:
            continue
        metrics = {}
        cpu = {}
        for key in ("frameThreadCpuUs", "processCpuUs"):
            a, b = before.get(key, -1), after.get(key, -1)
            same_thread = (before.get("frameThread") is not None
                           and before.get("frameThread") == after.get("frameThread"))
            if 0 <= a <= b and (key != "frameThreadCpuUs" or same_thread):
                cpu[key.replace("Us", "MsPerFrame")] = (b - a) / frames / 1000
        for name, value in after["metrics"].items():
            old = before["metrics"].get(name)
            if old is None:
                continue
            calls = value["calls"] - old["calls"]
            samples = value["samples"] - old["samples"]
            cost = value["sampledUs"] - old["sampledUs"]
            # No timed sample means unknown cost, not evidence of zero overhead.
            if min(calls, samples, cost) < 0 or samples == 0:
                continue
            metrics[name] = {
                "calls": calls, "samples": samples,
                "estimatedMsPerFrame": cost * calls / samples / frames / 1000,
            }
        yield {
            "endSeconds": after["elapsedUs"] / 1e6,
            "frameMs": elapsed / frames / 1000,
            "room": after["room"],
            "sameRoom": after["room"] == before["room"],
            "initialInterval": bool(before.get("initialBaseline")),
            "captureStart": after.get("captureStart"),
            "queuedPacks": after["queuedPacks"],
            "unconfirmedPacks": after.get("unconfirmedPacks"),
            "observedNativeBirthPacks": after.get("observedNativeBirthPacks"),
            "queuedCopies": after["queuedCopies"],
            "windowFrames": after["windowFrames"],
            "metrics": metrics,
            "scriptCoverage": after.get("scriptCoverage", {}),
            "cpu": cpu,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path)
    args = parser.parse_args()
    rows = [json.loads(line) for line in args.profile.read_text(encoding="utf-8").splitlines() if line.strip()]
    print(json.dumps({
        "complete": bool(rows and rows[-1]["final"]),
        "notice": "Sampled inclusive wall-clock estimates, not CPU attribution. Categories overlap; do not sum. Native work remains inside protected calls and the frame callback.",
        "intervals": list(intervals(rows)),
    }, indent=2))


if __name__ == "__main__":
    main()
