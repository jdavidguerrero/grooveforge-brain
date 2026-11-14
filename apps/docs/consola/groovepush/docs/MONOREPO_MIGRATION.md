# Monorepo Migration — GroovePush

_Date: 2026-07-16_

The two previously-separate repositories were unified into the **GroovePush** monorepo
while preserving their full commit history.

## Source repositories

| Original repo | New path | Remote (preserved) |
|---|---|---|
| `ableton_push_clone_gui` | `apps/gui` | https://github.com/jdavidguerrero/ableton_push_clone_gui.git |
| `ableton_push_clone_processor` | `apps/processor` | https://github.com/jdavidguerrero/ableton_push_clone_processor.git |
| `ableton_midi_api_integration` | `apps/remote-script` | https://github.com/jdavidguerrero/ableton_midi_api_integration.git |

- GUI & processor: branch `main`, 9 commits each, working trees clean.
- Remote Script: branch `main`, 14 commits. **Imported from the live Ableton folder**
  (`~/Music/Ableton/User Library/Remote Scripts/PushClone`), which has uncommitted
  local edits. The committed history was subtree-merged first, then the current
  working-tree state was synced on top (see the extra sync commit). The original
  Ableton folder was **left intact** because Ableton loads the script from there.

## Method

History was preserved using the `git subtree` merge recipe (not a flat copy):

```bash
git init -b main
# scaffolding commit ...
git remote add gui-src ./ableton_push_clone_gui
git fetch gui-src
git merge -s ours --no-commit --allow-unrelated-histories gui-src/main
git read-tree --prefix=apps/gui/ -u gui-src/main
git commit -m "feat: import GUI repo into apps/gui (full history preserved)"
git remote remove gui-src
# ...repeated for the processor under apps/processor
```

All original commits are reachable in `git log` (interleaved by author date). File
paths inside each app are unchanged relative to their old repo root — only prefixed
with `apps/gui/` or `apps/processor/`.

## Re-attaching an original remote (if needed)

To push a subtree back to its original standalone repo:

```bash
git subtree split --prefix=apps/gui -b gui-export
git push https://github.com/jdavidguerrero/ableton_push_clone_gui.git gui-export:main
```

## Notes / follow-ups
- Each app keeps its own `.gitignore`; the root `.gitignore` adds workspace-level rules.
- `apps/gui/CMakeLists 2.txt` is a stray duplicate carried over from the old repo —
  flagged in `GAPS_AND_IMPROVEMENTS.md` for cleanup.
- Per-app `package.json` wrappers were added so the pnpm workspace can orchestrate the
  native toolchains (CMake/Qt, PlatformIO).
