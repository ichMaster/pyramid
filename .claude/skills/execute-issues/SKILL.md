---
name: execute-issues
description: Execute GitHub issues for a version sequentially - implement, validate, commit, push, and generate a report.
---

# Skill: Execute GitHub Issues

Execute GitHub issues for a version sequentially: implement, validate, commit, push, and generate a report.

## Usage

```
/execute-issues <label> [--issue PYR-xxx] [--dry-run]
```

The `<label>` is the GitHub version label exactly as it appears (e.g., `v1::version:1`).

- `/execute-issues v1::version:1` -- execute all issues labeled `v1::version:1`
- `/execute-issues v1::version:1 --issue PYR-003` -- execute a single issue from that version
- `/execute-issues v1::version:1 --dry-run` -- show execution plan without making changes

## Instructions

### Step 0: Verify prerequisites

1. Confirm we are on the expected branch (e.g., `main` or the user's working branch)
2. Confirm working tree is clean (`git status`)
3. Confirm `gh` is authenticated
4. Parse the label to determine version:
   - Label `v1::version:1` -> version `n=1`
5. Fetch issues from GitHub:
   ```bash
   gh issue list --label "{label}" --state open --limit 100
   ```
6. Read the version issues file for detailed descriptions: `specification/roadmap/implementation/v{n}-issues.md`
7. If a GitHub report exists (`specification/roadmap/implementation/v{n}-github-report.md`), read the PYR-to-GitHub# mapping
8. Read [specification/ROADMAP.md](../../../specification/ROADMAP.md) for the version goal and the phase (`vA.B`) DoD, and [specification/ARCHITECTURE.md](../../../specification/ARCHITECTURE.md) for the contracts the issue must honor

### Step 1: Build execution queue

From the GitHub issue list, build an ordered queue based on dependencies:
- Parse PYR-xxx IDs from issue titles (format: `PYR-xxx: {title}`)
- Determine dependency order from the version issues file dependency tree
- Issues with no unmet dependencies go first
- Skip issues already closed on GitHub
- If `--issue PYR-xxx` is specified, execute only that issue (but verify its dependencies are closed)

Show the user the execution plan and ask for confirmation.

### Step 2: Execute each issue (loop)

For each issue in the queue:

#### 2a. Assign and announce

Print: `--- Starting PYR-xxx: {title} ---`

#### 2b. Read issue details

Read the full issue description from the version issues file (the detailed section for this PYR-xxx).

#### 2c. Implement

Execute the tasks described in the issue. Follow the project conventions in `CLAUDE.md` and the principles in `specification/MISSION.md`. Route by component:

- **Firmware changes** (`/firmware`): the device stays **thin** — no persona logic, memory, or decisions on-device (intelligence off-device). v0 builds in the **Arduino IDE**; from v1 the firmware is **PlatformIO** (`pio run`). Honor the WS/serial contracts in ARCHITECTURE.md exactly.
- **Server changes** (`/server`): Python, FastAPI + websockets. The ASR→LLM→TTS turn loop, auth, sessions, and the console live here. Behavior is defined by the role/config, not hardcoded.
- **MCP changes** (`/mcp`): the v3 `role`/`memory`/`knowledge_base`/`weather` services; the agent calls them all the same way.
- **Console changes** (`/console`): minimal web UI for role configuration.
- **Contract changes:** any change to a wire format (WS messages, activation, MCP tool schemas) updates `specification/ARCHITECTURE.md` AND its contract test, alongside both sides.
- Follow existing code style and patterns; keep each version self-contained (don't pull later-version concerns in early).

#### 2d. Validate

Run validation checks:

1. **Server / MCP unit + contract tests:** `pytest` for the changed packages (unit, plus the contract tests that pin WS / activation / MCP wire formats)
2. **Integration:** run the relevant full-turn integration test over the fake device + mock LLM/ASR/TTS (`text_in → reply`, or `audio → tts_end`)
3. **Syntax/import (Python):** `python3 -m py_compile {changed_py_files}` and an import check for changed modules
4. **Firmware build:** from v1, `pio run` (compile) and `pio test -e native` (host-testable parser/FSM/framing). In v0, note that compilation is via the Arduino IDE / `arduino-cli compile` and on-hardware behavior is a manual check.
5. **Contract consistency:** verify the WS/activation/MCP messages match ARCHITECTURE.md and the contract tests
6. **Acceptance criteria:** go through each criterion from the issue and verify against the phase DoD in ROADMAP.md

Record pass/fail for each check. **Tests are part of the work** — a feature lands with the tests that encode its acceptance (see ARCHITECTURE §Testing and CI).

Note: firmware on-device behavior (audio, LCD, Wi-Fi) requires the board; if unavailable, note that hardware validation is deferred to manual upload and cover host-testable logic with `pio test -e native`.

#### 2e. Commit

```bash
git add {specific files created/modified}
git commit -m "$(cat <<'EOF'
PYR-xxx: {title}

{1-2 sentence summary of what was implemented}

Closes #{github-issue-number}

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

#### 2f. Push

```bash
git push
```

#### 2g. Close issue with summary

```bash
gh issue close {issue-number} --comment "$(cat <<'EOF'
## Implementation Summary

**Commit:** {commit-hash}
**Files changed:** {count}

### What was done
{bullet list of key changes}

### Validation
{pass/fail status for each check}

### Acceptance criteria
{checklist with pass/fail}
EOF
)"
```

#### 2h. Log progress

Append to the in-memory execution log:
- Issue ID, title
- Commit hash
- Files changed (list)
- Validation results (including test pass/fail)
- Status: success/partial/failed

### Step 3: Handle failures

If implementation or validation fails for an issue:

1. Do NOT commit broken code
2. Stash or revert changes: `git checkout -- .`
3. Add a comment to the GitHub issue explaining what failed
4. Log the failure
5. Ask the user: continue to next issue (if no dependency), or stop?

### Step 3b: Version bump on completion

After ALL issues in the version are completed successfully (none failed, none remaining):

1. Determine the target semver from the version:
   - v0 -> `0.1.0`, v1 -> `0.2.0`, v2 -> `0.3.0`, v3 -> `1.0.0`

2. Update `README.md` with a version note if appropriate

3. Update or create `RELEASE.txt` -- prepend a new version entry:

```
Version {version} ({YYYY-MM-DD})
---------------------------
- {PYR-xxx title}: {1-sentence summary of what was implemented}
- {PYR-xxx title}: {1-sentence summary}
...
```

4. Commit the version bump:

```bash
git add README.md RELEASE.txt
git commit -m "$(cat <<'EOF'
Release v{version} -- {pyramid version vN} complete

All {count} issues implemented and validated.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

5. Tag the release:

```bash
git tag -a v{version} -m "v{n}: {version summary from ROADMAP}"
```

6. Report to user: `v{n} complete -> version bumped to {version}, tagged v{version}`

If some issues failed or were skipped, do NOT bump the version. Note in the execution report that the version is incomplete. (You can also delegate steps 3b–6 to `/release-version`.)

### Step 4: Generate execution report

After all issues are processed (or on stop), generate:
`specification/roadmap/implementation/v{n}-execution-report.md`

```markdown
# Version v{n} -- Execution Report

**Date:** {date}
**Branch:** {branch name}
**Label:** {label}
**Target version:** {version}
**Executed by:** Claude Code

## Summary

| Status | Count |
|--------|-------|
| Completed | {n} |
| Failed | {n} |
| Skipped | {n} |
| Remaining | {n} |

## Issues

| # | PYR ID | Title | Phase | Status | Commit | Files | Tests |
|---|--------|-------|-------|--------|--------|-------|-------|
| 1 | PYR-001 | Device skeleton and serial | v0.1 | completed | a1b2c3d | 2 | pass |
| ... | ... | ... | ... | ... | ... | ... | ... |

## Detailed Results

### PYR-001: Device skeleton and serial

**Status:** completed
**Commit:** a1b2c3d
**Files changed:**
- `firmware/...` (modified)

**Validation:**
- [x] Unit + contract tests: pass
- [x] Acceptance criteria: all pass
- [ ] Firmware on-device behavior: deferred to manual upload

---

### PYR-002: ...

## Next Steps

{List of remaining issues not yet executed, with their dependencies}
```

Commit and push this report:

```bash
git add specification/roadmap/implementation/v{n}-execution-report.md
git commit -m "$(cat <<'EOF'
Add v{n} execution report

{n} issues completed, {n} failed, {n} remaining.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
git push
```

## Important Rules

- **One issue at a time.** Never work on multiple issues simultaneously.
- **Dependency order.** Never start an issue whose dependencies are not closed.
- **Clean commits.** Each issue = one commit. No mixing work across issues.
- **No broken code.** Only commit code that passes validation (tests included).
- **Tests ship with the feature.** Every issue lands with the tests that encode its acceptance — no "tests later."
- **Intelligence off-device.** Never add persona logic, memory, or decisions to firmware.
- **Contracts stay stable.** A wire-format change updates ARCHITECTURE.md and its contract test in the same commit.
- **Config is the source of truth.** Don't hardcode behavior that belongs to the role/config.
- **Ask on ambiguity.** If an issue description is unclear, ask the user rather than guessing.
- **Progress updates.** Print a short status line after each issue completes.
