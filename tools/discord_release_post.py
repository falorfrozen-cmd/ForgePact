"""Post a published release's notes to the ForgePact Discord channel.

Run by `.github/workflows/discord-release.yml`:

    python tools/discord_release_post.py --tag v2.3.1 --name "ForgePact 2.3.1" \
        --url <release page> --notes-file notes.md [--no-everyone] [--dry-run]

The webhook address comes only from the DISCORD_WEBHOOK_URL environment
variable (the repository secret), never from an argument, so it cannot end up
in a log. Discord takes at most 2000 characters per message, so the notes are
split on line boundaries into as many messages as they need; the first one
starts with the @everyone ping and the last one ends with the release link.
Only the first message may ping (`allowed_mentions`), and a "@everyone" or
"@here" inside the notes themselves never pings.
"""

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request

LIMIT = 2000
# Room left in every message for the ping line or the link line.
CHUNK = 1800


def split_notes(notes: str, size: int = CHUNK) -> list:
    """Split `notes` into pieces of at most `size` characters, at line ends
    where possible; a single line longer than `size` is cut at a space, or
    hard when it has none. Blank lines at the edges of a piece are dropped."""
    pieces, current = [], ""
    for line in notes.replace("\r\n", "\n").split("\n"):
        while len(line) > size:
            cut = line.rfind(" ", 0, size)
            cut = cut if cut > 0 else size
            if current.strip():
                pieces.append(current.strip("\n"))
            current = ""
            pieces.append(line[:cut].rstrip())
            line = line[cut:].lstrip()
        candidate = f"{current}\n{line}" if current else line
        if len(candidate) > size:
            if current.strip():
                pieces.append(current.strip("\n"))
            current = line
        else:
            current = candidate
    if current.strip():
        pieces.append(current.strip("\n"))
    return pieces


def build_messages(name: str, url: str, notes: str, everyone: bool = True) -> list:
    """The webhook payloads, in posting order."""
    head = f"{'@everyone ' if everyone else ''}**{name} is out!**"
    tail = f"Download: <{url}>"
    pieces = split_notes(notes) or [""]
    messages = []
    for i, piece in enumerate(pieces):
        parts = [head] if i == 0 else []
        if piece:
            parts.append(piece)
        if i == len(pieces) - 1:
            parts.append(tail)
        content = "\n\n".join(parts)
        assert len(content) <= LIMIT, len(content)
        ping = everyone and i == 0
        messages.append({
            "content": content,
            "allowed_mentions": {"parse": ["everyone"] if ping else []},
        })
    return messages


def post(webhook: str, payload: dict) -> None:
    request = urllib.request.Request(
        webhook + "?wait=true",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json", "User-Agent": "ForgePact-release-bot"},
        method="POST",
    )
    for attempt in range(5):
        try:
            with urllib.request.urlopen(request, timeout=30):
                return
        except urllib.error.HTTPError as error:
            if error.code == 429 and attempt < 4:
                retry = json.loads(error.read() or b"{}").get("retry_after", 2)
                time.sleep(float(retry) + 0.5)
                continue
            raise SystemExit(f"Discord refused message: HTTP {error.code}")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--tag", required=True)
    parser.add_argument("--name", default="")
    parser.add_argument("--url", required=True)
    parser.add_argument("--notes-file", required=True)
    parser.add_argument("--no-everyone", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    with open(args.notes_file, encoding="utf-8") as handle:
        notes = handle.read()
    name = args.name.strip() or f"ForgePact {args.tag.lstrip('v')}"
    messages = build_messages(name, args.url, notes, everyone=not args.no_everyone)

    if args.dry_run:
        # Notes carry arrows and dashes; a Windows console's code page cannot
        # print them.
        if hasattr(sys.stdout, "reconfigure"):
            sys.stdout.reconfigure(encoding="utf-8")
        for i, message in enumerate(messages, 1):
            print(f"--- message {i}/{len(messages)} ({len(message['content'])} chars, "
                  f"mentions {message['allowed_mentions']['parse']})")
            print(message["content"])
        return 0

    webhook = os.environ.get("DISCORD_WEBHOOK_URL", "").strip()
    if not webhook.startswith("https://"):
        print("::error::DISCORD_WEBHOOK_URL is not configured. Add the repository secret and rerun.")
        return 1
    for i, message in enumerate(messages):
        if i:
            time.sleep(1.0)
        post(webhook, message)
    print(f"Posted {len(messages)} message(s) for {args.tag} to Discord.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
