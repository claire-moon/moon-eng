# MOON TEST COCKPIT

Status: portable controller, journal/summary writer, CGUI interface, and DOS
integration are implemented; the named Windows 98 and Windows NT physical
acceptance cases remain user-controlled.

`MOON.EXE /HITL HITL.IN` is the keyboard-only review surface for DOS builds.
It loads the ordered plan from `HITL.IN`, imports the separate AUTO lane from
`AUTO.OUT`, and always displays the run/profile identity with distinct AUTO,
MANUAL, and combined results. It does not turn automated success into human
approval.

## Authority boundary

CGUI owns focus, menus, help, and default-No confirmation. CGUI events carry
intent only and are deliberately safe to synthesize. The TEST COCKPIT
controller separately receives an input proof whose authority is fixed for the
complete session:

- ordinary `/HITL` is backed only by the live DOS IRQ1 adapter;
- `/HITL ... /SMOKE` is permanently synthetic and read-only; and
- zero-initialized, replayed, repeated, or held input never has MANUAL
  authority.

A MANUAL decision uses two distinct fresh input edges on different 35 Hz UI
ticks. The first edge selects PASS, FAIL, or BLOCKED and opens confirmation;
the second accepts the default-No modal. Before publication, the controller
rechecks the complete run/build/plan/profile identity and case binding. It then
appends the `MANUAL` journal record and flushes it. Only a successful append
publishes the new in-memory status. A failed append consumes the pending token
and leaves the prior MANUAL state unchanged.

```text
LIST -> DETAIL -> choose result -> default-No confirmation
                                      |
                         second fresh live DOS edge
                                      |
                         append + flush HITL.JRN
                                      |
                           publish MANUAL status
```

Stale AUTO identity disables MANUAL PASS. An explicit FAIL or BLOCKED may still
be recorded against a valid current plan identity for diagnosis, but the
combined result remains `BLOCKED / STALE_EVIDENCE`. Imported journal history
never restores current-process MANUAL authority; after an interrupted session,
the user repeats the physical decision.

## Interface

The case list is virtualized in ordered 12-row pages, so the 128-case evidence
limit never exceeds CGUI's 32-item menu capacity. Up and Down traverse the
complete plan with wrap; a page transition preserves the stable case ordinal.
Enter or Space opens the selected detail view. Each detail shows the complete
case title, required lanes, currentness, AUTO detail, MANUAL result, and stable
combined reason. F1 opens a one-page authority/key guide. Escape opens a
default-No commit-and-exit confirmation rather than discarding the session.

The header identifies whether the session is `LIVE DOS` or `SYNTHETIC READ
ONLY`. Stale identity is a text warning, not only a palette change. UI IDs are
session-local stable ordinals; journal and summary records always use the exact
case ID from `HITL.IN`.

## Journal and summary

`HITL.JRN` is opened in binary mode and is append-only within a run. Every
writer-produced line ends in CRLF and fits the 240-byte record limit. Event
sequence numbers are contiguous, event ticks never decrease, and a MANUAL
event requires live authority even when the lower-level journal API is called
directly.

The journal checksum is CRC-32/ISO-HDLC: reflected polynomial `0xEDB88320`,
initial state `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`. It covers every raw
canonical byte from the `HITL` header through the final EVENT CRLF. The
`JOURNAL` record count is the number of EVENT records; header and identity
records are not counted.

Commit first writes and closes `HITL.NEW`, reparses the candidate, and verifies
all identities, ordered results, combined codes, counts, journal CRC, and event
count. Only then does it replace the committed summary:

1. remove stale `HITL.OLD` when present;
2. rename an existing `HITL.OUT` to `HITL.OLD`;
3. rename verified `HITL.NEW` to `HITL.OUT`; and
4. remove `HITL.OLD` after success.

A failure before step 2 leaves `HITL.OUT` untouched. If the final rename fails,
`HITL.OLD` remains the recoverable prior summary. The host accepts only
`HITL.OUT`; `HITL.NEW` and `HITL.OLD` are never completed evidence.

## Automated and physical validation

The host and DJGPP suites test provenance combinations, two-edge confirmation,
stale identity, append-before-publish behavior, journal resume/CRC, summary
tampering, and every replace failure point. The vanilla DOSBox 0.74-3 smoke
navigates and renders, attempts a synthetic MANUAL decision, verifies that no
MANUAL event or status was created, commits an UNRUN summary, restores the DOS
video/input boundary, and returns control to the shell.

Automation does not complete the physical gate. On W98P90 and NT4P90, the user
runs `MOON.EXE /HITL HITL.IN` and assigns the `HITL.VISUAL`, `HITL.AUTH`,
`HITL.JOURNAL`, and `HITL.RESTORE` verdicts described in
[`TESTING.md`](TESTING.md).
