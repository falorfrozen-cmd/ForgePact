"""Post a published release's notes to the ForgePact Discord channel.

Run by `.github/workflows/discord-release.yml`:

    python tools/discord_release_post.py --tag v2.3.1 --name "ForgePact 2.3.1" \
        --url <release page> --notes-file notes.md [--no-everyone] [--dry-run]

The webhook address comes only from the DISCORD_WEBHOOK_URL environment
variable (the repository secret), never from an argument, so it cannot end up
in a log. The notes are written hard-wrapped, which GitHub reflows but Discord
shows line by line, so each paragraph and list item is joined back into one
line first (`unwrap`). Discord takes at most 2000 characters per message, so the
notes are then split between paragraphs and list items, never inside one, into
as many messages as they need. The first one is the official patch-notes
header (the @everyone ping, the title and the release date, in place of the
notes' own title), the others open with a part line, and the last one ends with
the download link. Each posted message's id is logged.
Only the first message may ping (`allowed_mentions`), and a "@everyone" or
"@here" inside the notes themselves never pings.
"""

import argparse
import datetime
import json
import re
import os
import sys
import time
import urllib.error
import urllib.request

LIMIT = 2000
# Room left in every message for the ping line or the link line.
CHUNK = 1800


LIST_ITEM = re.compile(r"^\s*([-*+]|\d+[.)])\s")


def starts_block(line: str) -> bool:
    """A line Markdown starts a new block with: a list item, heading, quote,
    table row or code fence (the line before a blank one is handled apart)."""
    s = line.lstrip()
    return bool(LIST_ITEM.match(line)) or s.startswith(("#", ">", "|", "```"))


def unwrap(notes: str) -> str:
    """Join the notes' hard-wrapped lines back into one line per paragraph or
    list item. GitHub reads a single line break inside a paragraph as a space;
    Discord shows every line break, so wrapped notes come out ragged there.
    Blank lines, headings, list items (with their indent), quotes, tables and
    fenced code keep their own lines."""
    out, in_code = [], False
    for line in notes.replace("\r\n", "\n").split("\n"):
        if line.lstrip().startswith("```"):
            in_code = not in_code
            out.append(line)
            continue
        if in_code or not line.strip() or starts_block(line) or not out:
            out.append(line.rstrip() if not in_code else line)
            continue
        prev = out[-1]
        if not prev.strip() or prev.lstrip().startswith(("#", "|", "```")):
            out.append(line.strip())
        else:
            out[-1] = prev.rstrip() + " " + line.strip()
    return "\n".join(out)


def cut_point(line: str, size: int) -> int:
    """Where to cut a line longer than `size`: after the last sentence end
    that fits, else at the last space, else hard at `size`."""
    for mark in (". ", "! ", "? ", "; ", ": "):
        at = line.rfind(mark, 0, size)
        if at > size // 3:
            return at + 1
    at = line.rfind(" ", 0, size)
    return at if at > 0 else size


def is_heading(line: str) -> bool:
    return line.lstrip().startswith("#")


def split_notes(notes: str, size: int = CHUNK) -> list:
    """Split `notes` (unwrapped first) into pieces of at most `size`
    characters. Pieces end between paragraphs or list items, never inside one,
    and never on a heading: trailing headings move on with their section, and
    a heading before a paragraph too long for one message opens that
    paragraph's first piece. Only such a paragraph is cut, at a sentence end."""
    pieces, current = [], []

    def joined(lines):
        return "\n".join(lines).strip("\n")

    def flush():
        # Close the current piece but keep its trailing headings (and the
        # blank lines around them) for the next one.
        nonlocal current
        body, carry = list(current), []
        while body and (not body[-1].strip() or is_heading(body[-1])):
            carry.insert(0, body.pop())
        if joined(body):
            pieces.append(joined(body))
        current = carry if joined(carry) else []

    for line in unwrap(notes).split("\n"):
        if len(joined(current + [line])) <= size:
            current.append(line)
            continue
        flush()
        while len(joined(current + [line])) > size:
            prefix = joined(current)
            room = size - len(prefix) - 2 if prefix else size
            if room < size // 4:
                # Headings alone nearly fill a piece: send them on their own.
                pieces.append(prefix)
                current = []
                continue
            cut = cut_point(line, room)
            chunk = line[:cut].rstrip()
            pieces.append(f"{prefix}\n\n{chunk}" if prefix else chunk)
            current = []
            line = line[cut:].lstrip()
        current.append(line)
    if joined(current):
        pieces.append(joined(current))
    return pieces


def release_date(published_at: str) -> str:
    """'2026-10-19T17:00:59Z' -> 'October 19, 2026'; '' when unknown."""
    try:
        when = datetime.datetime.strptime(published_at[:10], "%Y-%m-%d")
    except ValueError:
        return ""
    return f"{when:%B} {when.day}, {when.year}"


def drop_title(notes: str, name: str) -> str:
    """The notes open with their own `# ForgePact X` title; the announcement
    has its own, so a leading level-1 heading is dropped."""
    lines = notes.replace("\r\n", "\n").split("\n")
    first = next((i for i, l in enumerate(lines) if l.strip()), None)
    if first is not None and lines[first].startswith("# "):
        del lines[first]
    return "\n".join(lines)


def build_messages(name: str, url: str, notes: str, everyone: bool = True, date: str = "") -> list:
    """The webhook payloads, in posting order: an official patch-notes header
    on the first message, a part line on the others, the download link last."""
    header = (["@everyone"] if everyone else []) + [f"# \U0001F4E2 {name} \u2014 Official Patch Notes"]
    header.append(f"-# Released {date}" if date else "-# Official release")
    tail = f"\U0001F4E5 **Download {name}:** <{url}>"
    pieces = split_notes(drop_title(notes, name)) or [""]
    total = len(pieces)
    messages = []
    for i, piece in enumerate(pieces):
        if i == 0:
            parts = ["\n".join(header)]
        else:
            parts = [f"-# {name} patch notes \u00b7 part {i + 1} of {total}"]
        if piece:
            parts.append(piece)
        if i == total - 1:
            parts.append(tail)
        content = "\n\n".join(parts)
        assert len(content) <= LIMIT, len(content)
        ping = everyone and i == 0
        messages.append({
            "content": content,
            "allowed_mentions": {"parse": ["everyone"] if ping else []},
        })
    return messages


def post(webhook: str, payload: dict) -> str:
    """Send one message and return its Discord id (logged, so a post can be
    found and removed by hand later)."""
    request = urllib.request.Request(
        webhook + "?wait=true",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json", "User-Agent": "ForgePact-release-bot"},
        method="POST",
    )
    for attempt in range(5):
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                return str(json.loads(response.read() or b"{}").get("id", ""))
        except urllib.error.HTTPError as error:
            if error.code == 429 and attempt < 4:
                retry = json.loads(error.read() or b"{}").get("retry_after", 2)
                time.sleep(float(retry) + 0.5)
                continue
            raise SystemExit(f"Discord refused message: HTTP {error.code}")
    return ""


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--tag", required=True)
    parser.add_argument("--name", default="")
    parser.add_argument("--url", required=True)
    parser.add_argument("--notes-file", required=True)
    parser.add_argument("--published-at", default="", help="the release's published_at, for the date line")
    parser.add_argument("--no-everyone", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    with open(args.notes_file, encoding="utf-8") as handle:
        notes = handle.read()
    name = args.name.strip() or f"ForgePact {args.tag.lstrip('v')}"
    messages = build_messages(name, args.url, notes, everyone=not args.no_everyone,
                              date=release_date(args.published_at))

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
        print(f"message {i + 1}/{len(messages)} posted, id {post(webhook, message)}")
    print(f"Posted {len(messages)} message(s) for {args.tag} to Discord.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
