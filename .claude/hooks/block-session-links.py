#!/usr/bin/env python3
"""Reject commits, PR text and GitHub posts that link to the chat session."""
import json
import re
import sys

BANNED = re.compile(r"claude\.ai/code|^\s*Claude-Session:", re.IGNORECASE | re.MULTILINE)

data = json.load(sys.stdin)
tool = data.get("tool_name", "")
tool_input = data.get("tool_input", {})

if tool == "Bash":
    command = tool_input.get("command", "")
    # Only inspect commands that write history; reading/grepping is fine.
    if not re.search(r"\bgit\b.*\b(commit|tag|notes)\b", command):
        sys.exit(0)
    text = command
else:
    text = json.dumps(tool_input, ensure_ascii=False).replace("\\n", "\n")

if BANNED.search(text):
    sys.stderr.write(
        "Blocked: do not link to the chat session (no Claude-Session: trailer, "
        "no claude.ai/code URL) in commits, PR descriptions or comments. "
        "Remove it and retry.\n"
    )
    sys.exit(2)
