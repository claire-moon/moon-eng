# MOON TEST COCKPIT Evidence Format

Status: the portable `HITL.IN` / `AUTO.OUT` parser and combined-result
evaluator are implemented and tested on host, DJGPP, and vanilla DOSBox. The
CGUI cockpit, live-user journal writer, transactional summary writer, and host
collector remain subsequent Milestone 1 work.

`MOON.EXE /HITL HITL.IN` opens the keyboard-only **TEST COCKPIT** used for
guided DOS acceptance. This format keeps the user, Codex, CI, DOSBox, and 86Box
in one review loop without allowing automation to impersonate human approval.

## Authority and status

Every case has separate AUTO and MANUAL status fields. The only valid values are
uppercase `PASS`, `FAIL`, `BLOCKED`, and `UNRUN`.

- Automated tests and import tools may write AUTO status only.
- Only the user may assign MANUAL `PASS`, using an explicit TEST COCKPIT action
  while reviewing the named case.
- Codex, an agent, CI, a replay, or successful AUTO evidence may not write,
  infer, promote, or preserve a MANUAL `PASS` on the user's behalf.
- `FAIL` means the observed result conflicts with the acceptance statement.
- `BLOCKED` means the case could not be judged because a prerequisite or test
  environment failed.
- `UNRUN` is the initial effective lane state. Raw statuses from a completely
  parsed but stale `AUTO.OUT` may be retained for diagnosis, but cannot be
  applied to a case; its effective combined result is always
`BLOCKED / STALE_EVIDENCE`.

The portable evaluator binds each in-memory case state to the complete run,
build, plan, and profile identity plus its case ID. Combined evaluation checks
that binding against the live plan and `AUTO.OUT` every time. Applying AUTO
evidence for a different identity or case resets MANUAL to `UNRUN`; a manual
result is eligible only after the trusted cockpit binds it to the current
state.

The cockpit always displays AUTO and MANUAL status in separate columns and
shows build, plan, and profile identity on the same screen. It must require a
deliberate confirmation keystroke before recording MANUAL `PASS` or `FAIL`.

## Files

All names are DOS 8.3-safe and live in the writable test run directory.

| File | Owner | Purpose |
| --- | --- | --- |
| `HITL.IN` | host/CI | immutable run identity, selected cases, expectations |
| `AUTO.OUT` | automation | machine-produced result for each selected case |
| `HITL.JRN` | TEST COCKPIT | append-only interaction and status journal |
| `HITL.NEW` | TEST COCKPIT | fully written candidate summary |
| `HITL.OUT` | TEST COCKPIT | last committed summary returned to the host |
| `BUILD.TXT` | build system | artifact, compiler, package, and source identity |
| `CAPnnn.PCX` | TEST COCKPIT | optional numbered 320x200 evidence capture |

`nnn` ranges from `000` to `999`. A case refers to a capture by filename; image
data is never embedded in a TSV record.

When committing a summary, TEST COCKPIT closes and verifies `HITL.NEW`, removes
the prior transient `HITL.OLD`, renames an existing `HITL.OUT` to `HITL.OLD`,
renames `HITL.NEW` to `HITL.OUT`, and then removes `HITL.OLD`. If the final
rename fails, `HITL.OLD` remains recoverable and the journal records
`COMMIT_FAIL`. The host accepts only `HITL.OUT`; it never treats `HITL.NEW` or
`HITL.OLD` as completed evidence.

## Text encoding and lexical rules

- Control records are tab-separated ASCII. User-facing title and note fields
  may contain CP437 bytes `0x20` through `0xFE` except tab.
- Lines end in CRLF. Readers may accept LF input, but writers always emit CRLF.
- Blank lines and lines beginning with `#` are ignored on input.
- A record is at most 240 bytes including CRLF.
- A field may not contain tab, CR, LF, or NUL. TEST COCKPIT replaces invalid
  note bytes with `?` and records a warning.
- Decimal integers are unsigned and have no sign or leading whitespace.
- Hashes are lowercase hexadecimal. Identity hashes are SHA-256 (64 digits);
  Git commits are 40 lowercase hexadecimal digits; CRC-32 values are eight.
- Unknown record types cause `BLOCKED FORMAT_UNKNOWN`; they are not silently
  discarded. A newer minor format may add optional records whose names start
  with `X-`; older readers may ignore those records.

Every file begins with:

```text
HITL<TAB>1<TAB>0<CRLF>
```

The second and third fields are format major and minor. A reader rejects an
unsupported major version.

## Stable identity

Three SHA-256 values bind evidence to what was reviewed:

- `BUILD_HASH`: SHA-256 of the canonical `BUILD.TXT` bytes;
- `PLAN_HASH`: SHA-256 of the canonical selected-case plan represented by
  `HITL.IN`;
- `PROFILE_HASH`: SHA-256 of the normalized emulator/machine configuration.

`HITL.IN`, `AUTO.OUT`, `HITL.JRN`, and `HITL.OUT` repeat these three values. A
mismatch blocks the run as `STALE_EVIDENCE`. TEST COCKPIT must not offer manual
PASS while identities disagree. Changing an executable, MDP, selected case,
expected result, emulator, CPU, memory, video path, or audio path requires new
evidence.

The host computes SHA-256 identities. DOS code copies and compares them and may
use CRC-32 to catch local file damage; it is not required to implement SHA-256.

## `BUILD.TXT`

Required records, in this order after the header:

```text
SOURCE<TAB>0123456789abcdef0123456789abcdef01234567
COMPILER<TAB>djgpp-gcc<TAB>12.2<TAB>c99
ARTIFACT<TAB>ZEUS.EXE<TAB><sha256><TAB><size-bytes>
ARTIFACT<TAB>GAME.MDP<TAB><sha256><TAB><size-bytes>
MDP<TAB>1<TAB>0<TAB><directory-crc32>
CWSDPMI<TAB>CWSDPMI.EXE<TAB><sha256-or-NONE>
```

Additional shipped artifacts use more `ARTIFACT` records sorted by uppercase
8.3 filename. `BUILD_HASH` is computed after final CRLF normalization.

## `HITL.IN`

Required records after the header are:

```text
RUN<TAB>R260730A
BUILD_HASH<TAB><sha256>
PLAN_HASH<TAB><sha256>
PROFILE_HASH<TAB><sha256>
PROFILE<TAB>W98P90<TAB>Windows 98 SE<TAB>Pentium 90<TAB>16 MB<TAB>A220 I7 D1 H5
CASE<TAB>VID.START<TAB>1<TAB>1<TAB>ZEUS starts and displays the title screen
CASE<TAB>MENU.KEYS<TAB>0<TAB>1<TAB>Every main menu command is keyboard reachable
```

`RUN` is one to eight uppercase alphanumeric characters. The recommended form
is `RYYMMDDS`, where `S` distinguishes repeated runs on one date.

`PROFILE` fields are profile ID, operating system, CPU, memory, and BLASTER
configuration. Standard profile IDs are `DOSBOX`, `W98P90`, and `NT4P90`.

A `CASE` contains case ID, AUTO-required flag (`0` or `1`), MANUAL-required flag
(`0` or `1`), and title. At least one requirement flag must be `1`. Case IDs are
one to 24 characters from `A-Z`, `0-9`, `.`, `_`, and `-`. Cases occur in display
order and may not repeat.

`PLAN_HASH` is computed over the ordered `CASE` records, including their final
CRLF bytes. The hash field itself is therefore not part of the plan hash.

## `AUTO.OUT`

The identity preamble matches `HITL.IN`, followed by exactly one AUTO record per
selected case:

```text
RUN<TAB>R260730A
BUILD_HASH<TAB><sha256>
PLAN_HASH<TAB><sha256>
PROFILE_HASH<TAB><sha256>
AUTO<TAB>VID.START<TAB>PASS<TAB>OK<TAB>184<TAB>title state reached
AUTO<TAB>MENU.KEYS<TAB>UNRUN<TAB>MANUAL_ONLY<TAB>0<TAB>requires operator
```

AUTO fields are case ID, status, stable result code, elapsed milliseconds, and
short detail. A missing, duplicate, or unknown case record blocks import. A
manual-only case still receives an AUTO `UNRUN` record so absence is never
mistaken for success.

## `HITL.JRN`

The journal begins with the header and identity preamble. TEST COCKPIT appends
records and never rewrites earlier ones during a run:

```text
EVENT<TAB>1<TAB>0<TAB>OPEN<TAB>VID.START<TAB>case opened
EVENT<TAB>2<TAB>583<TAB>STEP<TAB>VID.START<TAB>title visible
EVENT<TAB>3<TAB>912<TAB>CAPTURE<TAB>VID.START<TAB>CAP001.PCX
EVENT<TAB>4<TAB>1140<TAB>MANUAL<TAB>VID.START<TAB>PASS
EVENT<TAB>5<TAB>1172<TAB>NOTE<TAB>VID.START<TAB>custom 60 Hz path selected
```

Fields are monotonically increasing sequence, elapsed simulation ticks since
cockpit start, event type, case ID (or `-` for run-wide events), and payload.
Recognized event types are `OPEN`, `STEP`, `CAPTURE`, `MANUAL`, `NOTE`,
`WARNING`, `COMMIT`, and `COMMIT_FAIL`.

Only a physical user action in the active cockpit may append a `MANUAL` event.
Imported journals never grant status authority. For repeated MANUAL events, the
last valid user event for the same build/plan/profile identity is authoritative
and the full history remains visible.

## `HITL.OUT`

The committed summary repeats identity, profile, and each selected case:

```text
RUN<TAB>R260730A
BUILD_HASH<TAB><sha256>
PLAN_HASH<TAB><sha256>
PROFILE_HASH<TAB><sha256>
PROFILE<TAB>W98P90
RESULT<TAB>VID.START<TAB>PASS<TAB>PASS<TAB>OK<TAB>CAP001.PCX
RESULT<TAB>MENU.KEYS<TAB>UNRUN<TAB>UNRUN<TAB>MANUAL_REQUIRED<TAB>-
SUMMARY<TAB>1<TAB>0<TAB>0<TAB>1
JOURNAL<TAB><crc32><TAB><record-count>
```

`RESULT` fields are case ID, AUTO status, MANUAL status, stable combined code,
and capture filename or `-`. `SUMMARY` fields count combined PASS, FAIL,
BLOCKED, and UNRUN cases in that order. `JOURNAL` identifies the journal used to
produce the summary.

Combined status is calculated as follows:

| Condition | Combined status/code |
| --- | --- |
| identity mismatch or invalid input | `BLOCKED / STALE_EVIDENCE` |
| either lane reports `FAIL` | `FAIL / AUTO_FAIL` or `MANUAL_FAIL` |
| a required lane reports `BLOCKED` | `BLOCKED / AUTO_BLOCKED` or `MANUAL_BLOCKED` |
| AUTO is required and is not run | `UNRUN / AUTO_REQUIRED` |
| MANUAL is required and is not run | `UNRUN / MANUAL_REQUIRED` |
| every required lane is `PASS` | `PASS / OK` |

For a non-manual case, MANUAL remains `UNRUN` and does not prevent combined
PASS. For a manual-only case, AUTO remains `UNRUN` and does not prevent combined
PASS. An optional lane that was actually run and reports `FAIL` still fails the
case. Any parser, write, capture, or commit failure is represented explicitly;
it can never default to PASS.

## Host collection

The host collector verifies format, identities, result counts, journal CRC,
capture references, and artifact hashes. It uploads the evidence directory as a
CI/PR artifact and reports the combined statuses to GitHub. Generated evidence
is not committed routinely to the source tree.

A new source commit or rebuilt artifact invalidates prior evidence. A release
candidate is eligible only when its required AUTO results pass and the exact
build/plan/profile combination has explicit user MANUAL PASS results.
