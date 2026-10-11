#!/usr/bin/env python3
"""After a PR is created or updated, make the agent read it back: the
server may append a session-link footer that no pre-call hook can see."""
import json
import sys

json.load(sys.stdin)
print(json.dumps({
    "hookSpecificOutput": {
        "hookEventName": "PostToolUse",
        "additionalContext": (
            "The server can append a session-link footer to PR text after "
            "this call. Read the PR back with pull_request_read (get) and, "
            "if its body contains a claude.ai link, update the body without "
            "it, then read it back again."
        ),
    }
}))
