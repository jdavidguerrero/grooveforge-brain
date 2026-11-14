---
description: Derive a small, ordered, verifiable task list from a GroovePush OpenSpec (SDD step 2)
argument-hint: <component>
---

You are running **SDD step 2 (TASKS)** for GroovePush.

Target component: **$1**.

Do this:
1. Read `specs/openspec/$1.md`. If it doesn't exist, tell the user to run `/spec $1` first.
2. Read `specs/templates/tasks-template.md`.
3. Break the spec's requirements into the **smallest sensible ordered steps**. Each task:
   - has an id `T-<COMPONENT>-n`,
   - cites the requirement(s) it satisfies (`R-<COMPONENT>-n`),
   - names the responsible **agent** (hardware-engineer / firmware-engineer /
     gui-engineer / remote-script-engineer / industrial-designer / integration-architect),
   - states an explicit **verification** (a runnable check with observable output),
   - starts unchecked (☐).
4. Order by dependency; a task must be doable given only earlier tasks are done.
5. Write to `specs/openspec/$1.tasks.md`.
6. Flag any requirement that is untestable as written and needs a spec fix.

Do NOT implement the tasks in this step — just produce the list and stop.
