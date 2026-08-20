# Goblin Engine — Template Dictionaries & Game Features Plan

- **Proposal file:** `modules/goblin/docs/rfc/native-game-features-rfc.md`
- **Date:** 2026-08-13
- **Status:** Proposed — consolidated; dictionary feature + recovered `then`/`elthen`
- **Base:** Godot 4.7.1-stable, fork plan v0.2.0

---

## 1. Purpose

Make Goblin Engine useful out of the box for the genre set's reference title. Primary deliverables:
1. **Expanded Dictionary** (typed entries, templates, callable members) — §2.
2. **Safe navigation & null coalescing** (`then` / `elthen`), recovered from the deleted `gdscript2` module — §3.

Secondary candidates (MIDI, lightmap baking, spatial field, palette) — §4.

**Naming (user-directed):** no `Goblin` prefix. Language features carry no class names; ported classes keep existing names; new native classes use plain names.

---

## 2. The Expanded Dictionary (Primary Feature)

> **Locked 2026-08-19 — design settled, IMPLEMENTED same day (backlog G-18).** G-18 = **`@schema` dictionaries**: `@schema` on a `const` Dictionary makes the const a project-wide reusable **schema**; `Dictionary[Name]` instantiates a Dictionary from it — defaults autofilled at construction, per-key type enforcement (wrong-type override = compile error), construction-time override merge (`= { key = value }`). Schema keys are typed; the dictionary stays **growable** beyond the schema (unknown keys fall back to the flat value type / `Variant`, G-17 rule; strict/fixed mode deferred). Rules: `@schema` requires a `const` (immutable schema) + a shaped Dictionary literal; a non-schema const in `Dictionary[...]` is an error. Engine surface (no `core/variant` change, no new keyword): extend const-as-type in `resolve_datatype` (analyzer const branches) + G-17 shape fields on `GDScriptDataType`/`DataType`; global schema registry (class_name-style scan-time collection, portable across files); single-arg `Dictionary[T]` type resolution; defaults vector serialized in `append_datatype`/`decode_datatype`; default-fill in `OPCODE_CONSTRUCT_SHAPED_DICTIONARY`. See §2.0 + backlog G-18.

### 2.0 `@schema` dictionaries (locked design)

```gdscript
@schema const mycritter = { hp: int = 10, name: String = "", pos: Vector2 = Vector2.ZERO }
var m: Dictionary[mycritter]                       # typed; defaults filled: hp=10, name="", pos=(0,0)
var m2: Dictionary[mycritter] = { hp = 20 }        # override merges: hp=20; types forced by schema
# var bad: Dictionary[mycritter] = { hp = "hello" } # ERROR — "hp" must be int
# var bad2: Dictionary[plain_const]                 # ERROR — not a schema (needs @schema)
var m3: Dictionary[mycritter] = { hp = 30, loot = ["sword"] }   # OK — grows past the schema
```

Semantics: a schema is a typed per-key shape + defaults; `Dictionary[Name]` is a Dictionary initialized from the schema; construction overrides merge and must match schema types; keys beyond the schema are allowed and grow the dictionary (G-17 extensibility); no fixed/strict mode yet. Schemas are `const`-only and registered project-wide at scan time (usable in any file, no imports). **IMPLEMENTED 2026-08-19 + hardened 2026-08-20 (backlog G-18)** — see `docs/gdscript_features.md` §Shaped Dictionary Literals. Registry is source-based (scan + reload re-sync after parse, before analysis), persisted to `res://.godot/goblin_schema_cache.cfg` (eager-loaded at init, shipped in exports); `Dictionary[Name]` is type-annotation-only (expression use = analyzer error). Known v1 limits: a project never opened in the editor run directly in game mode still needs load-order; inner-class schemas resolve within their own script only; no autocomplete wiring for schema names.

### 2.1 What it delivers

```gdscript
var creature_tpl := {
    hp: int = 10,
    reach: float = 1.5,
    tags: Array[String] = [],
}

var goblin := {
    hp = 25,          # override default; type stays int
    reach = 2.0,
}

goblin.reach      # typed float
```

### 2.2 Semantics (locked)

1. **Pre-set defaults (creation-time expansion):** template keys not set in the instance are added with their default value; materialized at construction — the runtime dictionary is a normal, fully-populated `Dictionary`.
2. **Types** come from the template (or an explicit entry annotation, which wins and is validated against the template type). Wrong-type override = analyzer error.
3. **Shape = template keys ∪ explicit keys**, recursing through nested shapes/containers.

### 2.3 Placement — why NOT `core/variant/dictionary.{h,cpp}`

Delivered in the **GDScript module** (mechanism #1), not by replacing the C++ `Dictionary` Variant class. Rationale: the override hook (mechanism #2) swaps compiled `.cpp` only, not headers, and `dictionary.h` is included everywhere; core Dictionary is a per-release rebase hotspot; templates/types/callables are language-layer concerns the fork already owns.

### 2.4 Phases

| Phase | What | Files (under `modules/goblin/modules/gdscript/`) | Effort |
|-------|------|------|--------|
| P0 — parser | `key: Type = value` (Lua style); speculative `parse_type()`+`=` lookahead | `gdscript_parser.{h,cpp}` | 1–1.5 d |
| P1 — analyzer | shape inference + template default merging + union-aware validation; remove nested-types `.clear()` at `gdscript_analyzer.cpp:2849` & `:2881-2882`; recurse | `gdscript_analyzer.cpp` | 2–3 d |
| P2 — runtime | recursive `_validate_against_datatype(Variant, GDScriptDataType)`; `GDScriptDataType` shape (`gdscript_function.h:48` already self-recursive); `OPCODE_CONSTRUCT_SHAPED_DICTIONARY` emits template-expanded typed dict; typed writes | `gdscript_compiler.cpp`, `gdscript_vm.cpp`, `gdscript_function.h` | 2–4 d |
| P3 — typed access + callable members | `dict.key` / `dict["key"]` / `dict.get()` refine to shape type; `dict.method()` invokes a stored `Callable`; autocomplete recurses | `gdscript_analyzer.cpp`, `gdscript_editor.cpp` | 3–5 d |
| P4 — callable shorthand (standalone) | `myCallable()` → `.call()`; `dict.member()` → `dict["member"].call()` for Callable-typed members | parser/analyzer/compiler | 1–2 d |

**Total: ~9–14 d.** Minimal first cut (P0+P1) ~3–4.5 d.

### 2.5 Callable shorthand

```gdscript
var fn: Callable = func(x: int) -> int: return x * 2
fn(3)                     # == fn.call(3)
var obj := { greet = Callable(func(n: String): print("hi " + n)) }
obj.greet("world")        # == obj["greet"].call("world")
```

### 2.6 Non-goals

No `core/variant/dictionary` replacement; no value semantics; no JSON-style typed entries; no runtime-attached template introspection.

### 2.7 Test gates

Parser (typed entries, style rules), analyzer (default merging, override/type precedence, nesting ≥ 3, union entries), runtime (clean `keys()`, typed-write errors, nested enforcement), callables. Reference corpus: 236 scripts + 342 tests compile/pass unchanged.

### 2.8 ADRs

- **ADR-0009** shaped dictionary literals (typed entries, recursive shape, access refinement).
- **ADR-0010** template dictionaries (creation-time expansion, GDScript-module placement).
- **ADR-0011** callable shorthand (`(...)` on Callable vars and dict members).

---

## 3. Safe Navigation & Null Coalescing (`then` / `elthen`) — Recovered

The feature existed in `modules/gdscript2/`, which was deleted in commit `5d2d167d9b` ("remove gdscript2 and create a fresh new copy of gdscript..."). Full implementation recovered from `git show 5d2d167d9b^:modules/gdscript2/<file>`. It uses the keywords `then` (safe navigation) and `elthen` (null coalescing), implemented as **binary operators** at `PREC_NULLISH` — no new VM opcodes (reuses the existing ternary codegen).

### 3.1 Recovered implementation map (exact sites)

| File (gdscript2) | Change |
|---|---|
| `gdscript_tokenizer.h:65-66` | enum `THEN`, `ELTHEN` |
| `gdscript_tokenizer.cpp:60-61` | token names `"then"`, `"elthen"` |
| `gdscript_tokenizer.cpp:506,536` | `KEYWORD("elthen", Token::ELTHEN)`, `KEYWORD("then", Token::THEN)` |
| `gdscript_parser.h:545-546` | `BinaryOpNode` enum: `OP_SAFE_NAVIGATE`, `OP_NULL_COALESCE` |
| `gdscript_parser.h` (precedence) | `PREC_NULLISH` inserted between `PREC_TERNARY` and `PREC_LOGIC_OR` |
| `gdscript_parser.cpp:3136-3141` | `parse_binary_operator`: `THEN → OP_SAFE_NAVIGATE`, `ELTHEN → OP_NULL_COALESCE` (both `variant_op = Variant::OP_MAX`) |
| `gdscript_parser.cpp:4271-4272` | precedence table rows: THEN/ELTHEN → `parse_binary_operator` @ `PREC_NULLISH` |
| `gdscript_analyzer.cpp:3139-3164` | `reduce_binary_op` special case: short-circuit typing; constant folding; result type |
| `gdscript_compiler.cpp:876-902` | `_parse_expression` BINARY_OPERATOR: two `case` branches using `write_start_ternary` / `write_ternary_condition` / `write_ternary_true_expr` / `write_ternary_false_expr` / `write_end_ternary` |

### 3.2 Semantics (verified against recovered code)

Verified in the recovered sources — the user's recollection is **inverted** relative to the code:

- **`a then b` (OP_SAFE_NAVIGATE) — TRUTHINESS, not null-only.** Analyzer constant-fold: `left_val.booleanize()`. Runtime: `write_ternary_condition` → `OPCODE_JUMP_IF_NOT` (booleanize). So `0 then b` → `b`, `false then b` → `b`, `null then b` → `null`. Result type = right's type (falls back to left's).
- **`a elthen b` (OP_NULL_COALESCE) — analyzer is NULL-ONLY, runtime is TRUTHINESS (internal mismatch).** Analyzer constant-fold: `left_val.get_type() != Variant::NIL` (null-only). Runtime: same `write_ternary_condition` → truthiness, with the in-code comment "We want left != null as the condition: reuse ternary by checking truthiness of left". So the *intent* was null-only, but the shipped codegen is truthiness: `0 elthen 5` → `5`, `"" elthen "x"` → `"x"`.

**Recommendation (decide at port time):** the standard meaning of both safe-navigation and null-coalescing is **null-only**. Neither operator currently emits strict null-only at runtime, so port them with an explicit `!= null` condition instead of reusing the ternary truthiness helper — this makes `then` null-only and aligns `elthen`'s runtime with its own analyzer. (If `then` should instead stay truthiness-based, only `elthen` gets the null check.)

> **Superseded 2026-08-13 (locked decision):** shipped semantics are final — `then` is null-only (`a != null ? b : a`), `elthen` is deliberately truthy (`a ? a : b`). The "wart" framing above no longer applies; do not change the code. See `modules/goblin/docs/gdscript_features.md` §then/elthen.

### 3.3 Porting to the current fork

**Port status: COMPLETE.** All items below have been applied to `modules/goblin/modules/gdscript/`. The "Confirmed absent" line describes the pre-port state only (what was missing from upstream before the port was applied).

The current fork (`modules/goblin/modules/gdscript/`) is the same 4.7.1 base as gdscript2, so the port is mechanical:

- Confirmed present in current fork: `write_start_ternary`/`write_ternary_*` codegen helpers (`gdscript_byte_codegen.h:494-498`), `OP_LOGIC_AND`/`OP_LOGIC_OR` enum (`gdscript_parser.h:551-552`), `PREC_TERNARY`/`PREC_LOGIC_OR` (`gdscript_parser.h:1524-1526`).
- Confirmed absent: `OP_SAFE_NAVIGATE`/`OP_NULL_COALESCE` enum values, `PREC_NULLISH`, `THEN`/`ELTHEN` tokens/keywords, the analyzer + compiler branches.

Work items (5 files, no VM changes): tokenizer (tokens + names + keywords), parser (enum + precedence + op mapping + table rows), analyzer (`reduce_binary_op` case), compiler (two `case` branches). Use `git diff` between the recovered gdscript2 files and the current upstream `modules/gdscript/` files to isolate each hunk, then apply into `modules/goblin/modules/gdscript/`.

### 3.4 Keywords vs symbols

User wants `then`/`elthen` back. Keep those keywords. Optional follow-up (fork plan Tier 0b): rename to `?.` / `??` syntax — more tokens + tokenizer ambiguity handling; do only if `then`/`elthen` prove unergonomic in the reference corpus.

### 3.5 Tests (none existed in gdscript2 — add)

New suite in `modules/gdscript/tests/` (~25 cases): basic `then`/`elthen`, constant folding, type inference, chaining, null vs falsy distinction (decides the §3.2 wart), interplay with union types, error cases (`then` on non-nullish types). Reference corpus compile gate.

### 3.6 Effort & risk

**~1–2 d**, low risk (additive operators, reuses existing codegen). Main decision point is the §3.2 truthiness-vs-null semantics.

---

## 4. Secondary Candidates (unchanged, for later)

| Candidate | Effort | Notes |
|-----------|--------|-------|
| Native `MidiStream` (+ `MidiFileResource`, `SoundFontResource`, importers) | 1–2 d | Port of GDExtension; TinySoundFont (MIT); names preserved; editor tooling `TOOLS_ENABLED`. |
| Native `LightmapBaker` (CPU runtime baker) | 2–4 d | Port of GDExtension; xatlas via namespace-wrapped TU to avoid editor duplicate symbols; works in export templates. |
| `SpatialField` / `LightField` | 3–5 d | Generic spatial field + CPU Thief-style light sampling; matches the reference-title API proposal. |
| `Palette` / `PaletteCycle` | 2–3 d | Native 256-color palette + C++ LUT gen + color cycling. |
| Typed-dictionary hardening (E1 union, E2 `.get()` refinement) | 0.5–2 d | E2 rides along with §2.4 P3. |
| `CompoundMeshInstance3D` | parked | Sources deleted from extension repo (`a95d3ac`); recover via git history if still needed. |

---

## 5. Rejected / Deferred

| Idea | Why |
|------|-----|
| Native `CustomTree` cadence scheduler | User: remnant, not necessary. |
| `GoblinDataTable` native fallback class | Replaced by §2 template dictionaries. |
| Replace `core/variant/dictionary.{h,cpp}` | Header override unsupported + max rebase surface; delivered in GDScript module (§2.3). |
| Expose editor `LightmapperRD` at runtime | Editor-only GPU module + forbidden build-flag changes; GL-compat templates lack RenderingDevice. |
| GDScript `extends Dictionary` / user Variant types | Variant + ClassDB surgery; §2 covers the need at language level. |
| Renderer-side light sampling on GL Compatibility | No exposed cluster seams; CPU `LightField` substitutes. |
| Native upscaler nodes | The reference title's GL-compat canvas shaders already do this. |

---

## 6. Build Integration

§2 and §3 live entirely in the existing GDScript fork (`modules/goblin/modules/gdscript/`, mechanism #1) — no SCsub/`register_types`/thirdparty changes. Secondary native classes use `modules/goblin/src/**/*.cpp` + `GDREGISTER_CLASS` (SCENE level; editor tooling `TOOLS_ENABLED`), thirdparty vendored with licenses.

**Risks:** analyzer/VM are upstream-hot — diff against `modules/gdscript/`, add one opcode rather than inlining into hot paths, run the full upstream `modules/gdscript/tests/` suite unchanged before each phase. Callable shorthand and `then`/`elthen` must not regress existing call/ternary error messages.

**Suggested start:** §3 (`then`/`elthen`, 1–2 d, lowest risk) → §2 P0+P1 (typed entries + template defaults) → decide P2–P4.
