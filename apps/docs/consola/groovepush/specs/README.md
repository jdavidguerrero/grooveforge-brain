# GroovePush — Spec-Driven Development (SDD) Harness

This project is **spec-first**. Nothing non-trivial gets built without a spec section it
traces back to. This folder is the source of truth that drives implementation.

## Layout

```
specs/
├── README.md                       ← this file (the workflow)
├── templates/
│   ├── openspec-template.md        ← structure for a feature/component spec
│   ├── tasks-template.md           ← ordered, verifiable task list
│   └── adr-template.md             ← architecture decision record
├── openspec/                       ← the living specs (source of truth)
│   ├── <component>.md              ← e.g. push-controller.md, gui.md
│   └── <feature>.tasks.md          ← task list derived from a spec
├── adr/                            ← accepted architecture decisions
├── push-controller-groovepilot.md  ← integration OpenSpec v1.0 (imported)
└── push-controller-gui-design.md   ← GUI design/brand spec (imported)
```

## The loop

```
   ┌──────────┐   ┌──────────┐   ┌────────────┐   ┌─────────┐   ┌─────────┐
   │  1 SPEC  │──▶│ 2 TASKS  │──▶│ 3 IMPLEMENT│──▶│ 4 VERIFY│──▶│ 5 REVIEW│
   └──────────┘   └──────────┘   └────────────┘   └─────────┘   └─────────┘
        ▲                                                             │
        └──────────────── spec updated if reality differs ───────────┘
```

1. **SPEC** — Write or amend an OpenSpec in `openspec/<component>.md` using the template.
   State the *what* and the *why*, acceptance criteria, and inviolable constraints.
   Command: `/spec <component>`.
2. **TASKS** — Break the spec into a **small, ordered** list of verifiable tasks in
   `openspec/<component>.tasks.md`. Each task cites the spec section it satisfies and its
   verification. Command: `/tasks <component>`.
3. **IMPLEMENT** — Do **one task at a time**. Route work to the right agent
   (`hardware-engineer`, `firmware-engineer`, `gui-engineer`, `remote-script-engineer`,
   `industrial-designer`, `integration-architect`).
4. **VERIFY** — Exercise the **real** behavior, not just a build:
   - firmware → a `test_*` env on the bench + serial monitor,
   - GUI → run `appPushClone`,
   - Remote Script → reload in Live + watch logs,
   - hardware → KiCad ERC/DRC.
5. **REVIEW** — `/hw-review` for schematics, `/code-review` for code. Then check the box
   in the tasks file and, if reality diverged from the spec, update the spec.

## Rules

- **Traceability**: every PR/commit references a spec section or a task id.
- **Contracts are shared**: a protocol change updates firmware + Remote Script + GUI and
  the contract doc *in the same change*, and bumps the contract version.
- **Inviolable principles** (root `CLAUDE.md`) override any task convenience.
- Keep specs declarative (behavior, constraints, acceptance) — not code dumps.
- A task is "done" only with **evidence** of verification.

## Naming

- Spec: `openspec/<component>.md` — id `SPEC-<COMPONENT>`.
- Requirement id inside a spec: `R-<COMPONENT>-<n>`.
- Task id: `T-<COMPONENT>-<n>` (in `<component>.tasks.md`).
- ADR: `adr/NNNN-title.md`.
