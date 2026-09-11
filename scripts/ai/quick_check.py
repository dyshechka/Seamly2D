#!/usr/bin/env python3
"""
Fast pre-build sanity check for Seamly2D changes.

Why this exists: this project can only be *built* through Qt Creator's GUI
on macOS (see CLAUDE.md) — there is no command-line qmake/make available to
an AI session working through the device bridge. A full GUI build-and-run
cycle costs real time. This script catches the cheap, common mistakes
*before* that cycle: malformed .ui XML, and unbalanced braces/parens in
touched .cpp/.h files. It is NOT a compiler and cannot catch real C++ or Qt
errors — it only rules out a class of silly mistakes early.

Usage:
    python3 scripts/ai/quick_check.py                 # check files changed vs 'develop'
    python3 scripts/ai/quick_check.py path/to/file.ui path/to/file.cpp
    python3 scripts/ai/quick_check.py --base main      # diff against a different base branch

Exit code is non-zero if any check fails.
"""
import argparse
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def changed_files(base: str) -> list[str]:
    try:
        out = subprocess.run(
            ["git", "diff", "--name-only", "--diff-filter=ACMR", f"{base}...HEAD"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=True,
        ).stdout
    except subprocess.CalledProcessError as exc:
        print(f"Could not diff against '{base}': {exc.stderr.strip()}", file=sys.stderr)
        return []
    return [line.strip() for line in out.splitlines() if line.strip()]


def check_ui_xml(path: Path) -> list[str]:
    """.ui files are XML (Qt Designer forms) — must parse cleanly."""
    try:
        ET.parse(path)
    except ET.ParseError as exc:
        return [f"malformed XML: {exc}"]
    return []


def check_brace_balance(path: Path) -> list[str]:
    """
    Cheap balance check for {}, (), [] across a C++ file, ignoring braces
    inside string/char literals and // and /* */ comments. This is a
    heuristic, not a real tokenizer — it exists to catch an accidentally
    dropped/duplicated brace while editing, not to validate syntax.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    pairs = {"{": "}", "(": ")", "[": "]"}
    closers = {v: k for k, v in pairs.items()}
    stack: list[str] = []
    i, n = 0, len(text)
    in_line_comment = in_block_comment = False
    in_string = in_char = False
    errors: list[str] = []
    line = 1

    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if c == "\n":
            line += 1
            in_line_comment = False
            i += 1
            continue

        if in_line_comment:
            i += 1
            continue
        if in_block_comment:
            if c == "*" and nxt == "/":
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue
        if in_string:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue
        if in_char:
            if c == "\\":
                i += 2
                continue
            if c == "'":
                in_char = False
            i += 1
            continue

        if c == "/" and nxt == "/":
            in_line_comment = True
            i += 2
            continue
        if c == "/" and nxt == "*":
            in_block_comment = True
            i += 2
            continue
        if c == '"':
            in_string = True
            i += 1
            continue
        if c == "'":
            in_char = True
            i += 1
            continue

        if c in pairs:
            stack.append((c, line))
        elif c in closers:
            if not stack or stack[-1][0] != closers[c]:
                errors.append(f"line {line}: unexpected '{c}'")
            else:
                stack.pop()
        i += 1

    for opener, at_line in stack:
        errors.append(f"line {at_line}: unclosed '{opener}'")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("files", nargs="*", help="specific files to check (default: files changed vs --base)")
    parser.add_argument("--base", default="develop", help="branch to diff against when no files are given (default: develop)")
    args = parser.parse_args()

    targets = args.files or changed_files(args.base)
    if not targets:
        print("Nothing to check.")
        return 0

    had_errors = False
    for rel in targets:
        path = REPO_ROOT / rel
        if not path.exists():
            continue  # deleted file
        suffix = path.suffix.lower()
        errors: list[str] = []
        if suffix == ".ui":
            errors = check_ui_xml(path)
        elif suffix in (".cpp", ".h", ".hpp", ".cc"):
            errors = check_brace_balance(path)
        else:
            continue

        if errors:
            had_errors = True
            print(f"FAIL {rel}")
            for err in errors:
                print(f"  - {err}")
        else:
            print(f"ok   {rel}")

    return 1 if had_errors else 0


if __name__ == "__main__":
    sys.exit(main())
