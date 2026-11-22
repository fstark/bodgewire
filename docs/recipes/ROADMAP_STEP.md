# Recipe — A roadmap step's spec, design and plan

The three documents of a step `PXy` in `docs/pXy-name/`, named after the step (`LAUNCHER_LOOK.md`, `LAUNCHER_LOOK_DESIGN.md`, `LAUNCHER_LOOK_PLAN.md`). The procedure around them (ask first, review, implement, finish) is in `AGENTS.md`. A recipe step is cited, never repeated ("74LS90: per ADD_PART.md").

Model: P7b, the Launcher look (`7321144` → `b5c7abb`; the docs as written before phase 1 in `git show 7321144^:docs/p7b-launcher-look/`).

## Spec: `NAME.md`

```
# PXy — Title

Roadmap step PXy (ARCHITECTURE.md#roadmap): one or two sentences, what changes. Design: … Plan: …

## Why
What the player or the project gains; what was wrong before.

## User decisions
From the roadmap row and the questions asked (the user, YYYY-MM-DD): one bullet a decision.
Chosen without asking: the choices left to the agent, so the user can overturn them.

## What the player sees            (or What it does, for a step with nothing on screen)
Numbered items (§1, §2…) the design and tests cite; subsections where it helps.

## What is new
Numbered blocks, settled file by file in the design. What stays untouched (kernel, Parts, Machines…).

## Tests
Unit tests (case names), Machine cases, screenshots (release, viewed: which and what they show),
speed (ratio, UI ms), sanitizers (debug, which runs), what only the user can check, the full release suite.

## Later
What was left out, for a later row.
```

## Design: `NAME_DESIGN.md`

```
# PXy — Title — Design

How NAME.md is built. Plan: … Numbers in parentheses (§3) are the spec's items.
One paragraph: what is untouched, what is new, what changes.

## Files
| File | Change |            every file created or changed, `(new)` marked

## 1. Block (§n)              one section a spec block
### `src/x/y.hpp`             the interface as code (types, signatures, constants), the rule in prose
### Tests (`tests/x/y_test.cpp`, cases `y: …`, `[tag]`)

## N. Docs
GLOSSARY, recipes, ARCHITECTURE: what each gains.

## Checks
The fences' checks gathered: which ctest -R, which screenshots, which debug runs.
```

Concrete: real paths, real function names, checked against the code (the review does it again). No rejected ideas unless one is a trap.

## Plan: `NAME_PLAN.md`

```
# PXy — Title — Implementation Plan

Spec: … Design: …

## Status
| Phase | Description | Commit |

## Phase 1 — Title
1. What to do, citing the design's sections (§1).
2. …

Fence: the checks (release / debug `ctest -R "^(a|b): "`; the full release suite when shared code
changed; screenshots viewed). Commit: `module: what changed (PXy phase 1)`, its body naming the checks.
```

- Every phase builds and passes its checks; each is one commit, in order, without stopping (AGENTS.md).
- The last phase: docs (GLOSSARY, recipes fixed, ARCHITECTURE row), release check with screenshots viewed, full release suite.
- As phases land: the commit in the status table, and one `## Implementation notes` bullet a phase (date, what differed from the design, the checks run). The review's must-fixes go there too, as the first bullet.
- At the end the plan is cut to the status table and the notes.
