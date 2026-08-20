# De-Submodule: Inline `modules/goblin` into the Main Repo — Architecture Analysis + Migration Plan

Status: analysis complete, implementation-ready. NOT yet implemented (analysis-only task).

## 1. Verdict

**INLINE.** Convert `modules/goblin` from a git submodule to a plain tracked directory in
goblinengine/godot, preserving the 46-commit history via `git subtree add`. Archive
goblinengine/goblin on GitHub (ecs branch preserved).

Rationale in one sentence: the submodule boundary buys nothing the fork uses, and it is
currently costing correctness — the recorded gitlink (cceaa4c) is 5 commits stale against the
checked-out goblin main (12bfd67), so any fresh clone/CI checkout builds a goblin that is
missing the entire dictionary-schema feature, and the ADR 0002 divergence-measurement command
(`git diff upstream/4.7 -- modules/goblin/modules/gdscript modules/gdscript`) is a silent
no-op today because git cannot descend into a gitlink pathspec.

## 2. Evidence gathered (verified)

| # | Claim | Verification |
|---|-------|--------------|
| E1 | Single gitlink: `160000 cceaa4c... modules/goblin` | `git ls-files -s` — only one `160000` entry |
| E2 | Stale pointer: worktree HEAD 12bfd67 vs gitlink cceaa4c, +5 commits (12bfd67, b4c30cf, 1d1bb7f, 2cefa84, af3d530) | `git submodule status` shows `+`; `git -C modules/goblin log cceaa4c..HEAD` |
| E3 | Submodule worktree clean | `git -C modules/goblin status --short` empty |
| E4 | Build integration is path-relative (submodule-agnostic) | `config.py`: `os.path.dirname(__file__)`; `SCsub`: `Dir(".").abspath`; no submodule assumptions anywhere in build code |
| E5 | Upstream has NO `modules/goblin/` path | `git ls-tree upstream/4.7 modules/` → no goblin; `git ls-tree upstream/4.7 modules/goblin` → empty |
| E6 | `.gitmodules` lists ONLY modules/goblin → upstream Godot has no submodules in this fork | read `.gitmodules` (3 lines) |
| E7 | No ADR references the submodule boundary — all reference the PATH `modules/goblin/` | read adr/0001,0002,0003,0007,0008,0009,0012 + adr/README.md |
| E8 | rules.md / master_prompt.md / overrides skill / porting skill / .kilo commands reference paths only | grep across `.kilo/` |
| E9 | Porting-skill step-2 diff is a silent no-op for the goblin side today | `git diff upstream/4.7 -- modules/goblin/modules/gdscript modules/gdscript` → empty output |
| E10 | Orphan submodule config sections in `.git/config`: `submodule.modules.daslang`, `submodule.modules.gdscript2` (no gitlink, no dir, not in .gitmodules) | `git config --get-regexp submodule`; `Test-Path modules/daslang|gdscript2` False |
| E11 | CI: only `.github/workflows/goblin_builds.yml` (fork-owned) references goblin content; it uses `submodules: recursive` 5x | grep + read goblin_builds.yml; upstream workflows never mention goblin |
| E12 | Fork already flipped inline↔submodule twice (b5168f5a9f add-submodule; later inlined; b433bb1a83 "turn goblin back into a submodule") | `git log --all -- .gitmodules` |
| E13 | goblin repo has no `.github/` workflows; root holds README/INDEX/.gitignore/.github-workflow-example.yml + logos | `ls-tree --name-only HEAD` + Test-Path |
| E14 | ecs branch: merge-base 12bfd67, +2153/-107 across 29 files vs main | `git -C modules/goblin diff --stat main..origin/ecs` |
| E15 | Main `.gitignore` already ignores `__pycache__/` (line 77); no `*.gen.h` ignore | Select-String .gitignore |
| E16 | Submodule `.git` is a pointer file → `gitdir: ../../.git/modules/modules/goblin` | read `modules/goblin/.git` |
| E17 | Reference-title-side hazard: fresh clone builds cceaa4c (missing dictionary-schema work), CI/worktree divergence | consequence of E2 |

## 3. Risk evaluation

### (a) Porting / rebase workflow impact — LOW, net POSITIVE
- Upstream commits never touch `modules/goblin/` (E5). A subtree-inlined directory has the
  same zero-overlap property a submodule has. `git rebase --rebase-merges` onto a new stable
  base replays the subtree merge commit without conflict.
- Improvement: E9 — today the ADR 0002 divergence-measurement command cannot see goblin
  content (git treats the submodule as one gitlink object). After inlining, the exact command
  the porting skill documents becomes functional. ADR 0002's core workflow is currently
  impaired by the boundary; inlining repairs it.
- `git log -- modules/goblin/` (used by /report, /feature-review, tech-debt-review) currently
  shows only pointer-bump commits (e.g. 6e189a9dd0, 059527e3bb, b53f071980, 85895eea4a).
  After inline it shows real goblin commits. Commands improve with zero edits.

### (b) History graft mechanics for 46 commits — LOW
- Correct tool: `git subtree add --prefix=modules/goblin <repo> main` — replays all 46 commits
  (prefixed trees, rewritten hashes, original messages/authors/dates) and creates a merge
  commit. `git merge -s ours` alone does NOT work (it discards the incoming tree — history
  ancestors yes, content no). The proposal's "merge -s ours" must be read as "the ours-style
  merge that `git subtree add` performs".
- Precondition: `git rm --cached modules/goblin` first — `git subtree add` fails if the gitlink
  still occupies the prefix in the index (E1).
- Use the local submodule path as the subtree source to avoid network fetch:
  `git subtree add --prefix=modules/goblin D:/DEV/git/godot/modules/goblin main`.
- 46 commits is trivially small for subtree.

### (c) Reversibility — HIGH (good)
- `git subtree split --prefix=modules/goblin -b goblin-restored` regenerates the full goblin
  repo (all 46+ commits); push to re-establish a standalone repo or submodule.
- goblinengine/goblin stays live as an archive (E14: ecs branch preserved online).
- History exists in two places; re-submoduling later is a documented, low-risk operation.

### (d) ADR dependency on the submodule boundary — NONE
- E7: every ADR references the PATH. 0001 (mechanisms), 0002 (diff workflows), 0003/0012
  (config.py trim), 0007 (editor mirrors), 0008 (additive modules) all survive verbatim.
- ADR 0008 actually argues FOR inlining: it moved additive modules (midi, sim, betsy) into the
  main repo, leaving the override layer as the sole separate repo — the odd one out.
- New ADR 0013 records the decision and prevents a third flip (E12).

## 4. Decision-hierarchy mapping

| Principle | Impact |
|-----------|--------|
| 1. Hot-path performance | Neutral — build-time mechanisms only, unchanged |
| 2. Reference-title compatibility | Positive — eliminates the stale-pointer hazard (E17): fresh clones/CI build the intended goblin; no `--recursive` prerequisite |
| 3. Minimal override surface | Neutral — same mechanisms, same paths, same dicts (ADR 0001/0007) |
| 4. Lean scope | Positive — removes pointer-bookkeeping commits, the broken porting measurement (E9), and the two-repo workflow |
| Non-negotiable: Godot compatibility | Unaffected — E4 proves path-relative integration |
| Non-negotiable: rebase-friendliness | Equal-or-better — E5 zero upstream overlap; measurement now real |

## 5. Migration steps (exact)

Phase 0 — snapshot:
```
git -C modules/goblin status --short          # must be empty (E3)
git -C modules/goblin rev-parse HEAD          # 12bfd67 — THIS is what we inline (not cceaa4c)
```

Phase 1 — de-index gitlink + config, then graft:
```
git rm --cached modules/goblin
git rm .gitmodules
git config --remove-section submodule.modules.goblin
git config --remove-section submodule.modules.daslang      # orphan (E10)
git config --remove-section submodule.modules.gdscript2    # orphan (E10)
git subtree add --prefix=modules/goblin D:/DEV/git/godot/modules/goblin main
```
The staged deletions ride into the subtree merge commit → single transition commit.

Phase 2 — strip submodule residue (untracked; no commit needed):
```
Remove-Item modules/goblin/.git                                # pointer file (E16)
Remove-Item -Recurse -Force .git/modules/modules               # submodule gitdir
Remove-Item -Recurse -Force modules/goblin/__pycache__         # covered by E15 anyway
```
Keep `modules/goblin/.gitignore` (scopes correctly to the subtree) and the root
`.github-workflow-example.yml` (harmless; optional later prune).

Phase 3 — CI:
- `.github/workflows/goblin_builds.yml`: remove `submodules: recursive` from all 5 checkout
  steps.
- Do NOT touch upstream workflows (windows/linux/macos/web/android/ios_builds.yml) or
  `.github/actions/godot-cpp-build/action.yml`: after `.gitmodules` deletion their
  `submodules: recursive` is a harmless no-op (E6, E11), and editing upstream files violates
  rules.md hard rule 2.

Phase 4 — archive:
- GitHub goblinengine/goblin → Settings → Archive. ecs branch preserved (E14).

Phase 5 — docs (goblin tree, now in main):
- New `modules/goblin/docs/adr/0013-inline-goblin-into-main-repo.md`.
- `modules/goblin/docs/adr/README.md`: add 0013 row to the accepted list.
- `modules/goblin/docs/backlog.md`: B-21 row (de-submodule migration) → done.
- No edits: STRUCTURE.md, INDEX.md, goblin README.md (E7 — no submodule references).
- `rules.md`: no rule change (paths stay valid). Optional one-line Orientation note citing
  ADR 0013 to prevent a third flip.
- `master_prompt.md`: NO edit — no principle/hierarchy/non-negotiable changed (living-charter
  rule not triggered).

Phase 6 — verification gates:
- Content equivalence: `git diff 12bfd67 $(git rev-parse HEAD:modules/goblin)` must be empty
  (inlined tree == pre-migration goblin tree, byte-for-byte).
- Build gate (build skill): `scons platform=windows target=editor module_mono_enabled=no
  accesskit=no angle=no debug_symbols=yes -j4` → success, trim canary prints, binary runs.
- Reference-title gate: corpus compile + 342 tests + level load.
- CI gate: workflow_dispatch on goblin_builds.yml produces `bin/goblin.windows.*.exe` from a
  submodule-free checkout.

## 6. Proposed ADR 0013 (draft)

Title: **De-Submodule: Inline the Goblin Override Layer into the Main Repository**

Rationale: The override layer's home is a path, not a repository boundary: every ADR
(0001/0002/0003/0007/0008/0009/0012) and every rule references `modules/goblin/` as a
directory, and all build integration is path-relative. The submodule boundary therefore adds
operational overhead without adding a capability — and it is demonstrably failing: the
recorded gitlink (cceaa4c) is 5 commits stale against the checked-out goblin main (12bfd67),
so fresh clones and CI builds a goblin missing the dictionary-schema feature, and git's
inability to descend into gitlink pathspecs silently breaks the ADR 0002
divergence-measurement command. Inlining `modules/goblin/` as a plain tracked directory via
`git subtree add` (preserving all 46 commits in the fork's ancestry) makes the fork's entire
divergence surface version together, consistent with ADR 0008's additive modules, keeps
reversibility via `git subtree split`, and archives goblinengine/goblin (ecs branch preserved)
so the history remains online.

## 7. File checklist

| File / artifact | Action |
|-----------------|--------|
| `.gitmodules` | delete |
| `.git/config` submodule sections (goblin, daslang, gdscript2) | remove (local) |
| index gitlink `modules/goblin` | `git rm --cached` |
| `modules/goblin/` (46 commits) | `git subtree add` — grafted |
| `.github/workflows/goblin_builds.yml` | drop 5× `submodules: recursive` |
| `modules/goblin/.git` | delete pointer file |
| `.git/modules/modules/` | delete gitdir |
| `modules/goblin/__pycache__/` | delete |
| `modules/goblin/docs/adr/0013-inline-goblin-into-main-repo.md` | new ADR |
| `modules/goblin/docs/adr/README.md` | add 0013 row |
| `modules/goblin/docs/backlog.md` | B-21 → done |
| `.kilo/rules/rules.md` | optional Orientation note (ADR 0013) |
| `.kilo/rules/master_prompt.md` | NO change |
| `.kilo/skills/*`, `.kilo/command/*` | NO change (path-only; improve automatically) |
| github.com/goblinengine/goblin | archive (ecs preserved) |

## 8. Accepted risks (explicit)

- Third structural flip (E12) — consequence: churn cost. Mitigation: ADR 0013 locks the model;
  reversibility is a documented `subtree split` away.
- Future maintainers wanting per-path commit governance on goblin code — consequence: lost
  without repo-level rules. Mitigation: GitHub branch/path protection on `modules/goblin/**`
  if ever needed; not required today.
- The goblin repo's own README/.gitignore/example-workflow merge into main — consequence:
  minor dead weight. Accepted; keeps the merge faithful to source.
