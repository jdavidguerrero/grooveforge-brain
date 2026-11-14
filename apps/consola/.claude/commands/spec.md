---
description: Create or amend an OpenSpec for a GroovePush component (SDD step 1)
argument-hint: <component> [what to spec]
---

You are running **SDD step 1 (SPEC)** for GroovePush.

Target component: **$1**. Extra intent: $ARGUMENTS

Do this:
1. Read `specs/README.md` (the workflow) and `specs/templates/openspec-template.md`.
2. If `specs/openspec/$1.md` exists, read and amend it; otherwise create it from the
   template with id `SPEC-<COMPONENT>`.
3. Ground the spec in reality: read the relevant source (`apps/*`, `include/shared/*`,
   `PIN_DISTRIBUTION.md`, contract docs) and the higher-level specs
   (`specs/push-controller-groovepilot.md`, `specs/push-controller-gui-design.md`).
4. Fill in: purpose, scope, **inviolable constraints** (cross-check the four principles in
   `CLAUDE.md`), numbered testable requirements (`R-<COMPONENT>-n`), interfaces/contracts
   with source-of-truth citations, and acceptance criteria.
5. Keep it declarative — behavior and constraints, not code.
6. End by listing open questions and proposing whether to proceed to `/tasks $1`.

Do NOT write implementation code in this step.
