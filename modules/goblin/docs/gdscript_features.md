# GDScript Fork Features

The GDScript fork lives at `modules/goblin/modules/gdscript/` and is compiled instead of upstream `modules/gdscript/` via the whole-module override (ADR 0001).

This file documents the divergence from upstream GDScript. When porting across engine versions, diff `modules/goblin/modules/gdscript/` against `modules/gdscript/`. When changing code, update this file + `docs/CODE_MAP.md`.

## Features

### Union Types

Syntax: `int | float`, `Dictionary | null`, `var x: null | null` (redundant members dedup; single-member unions collapse to the member type)

The `null` type is a singleton: only `null` can be assigned/converted to it. `check_type_compatibility` has a guard because upstream `Variant::can_convert_strict` reports every type as convertible to NIL ("nil can convert to anything") — that rule is for the internal NIL Variant and would silently accept `x = 5` on a `null`-typed variable.

- Parser: `parse_type()` / `parse_type_single()`, `DataType::UNION` kind.
- Analyzer: `resolve_datatype()`, `check_type_compatibility()`, cast reduction.
- Compiler: maps `UNION` → runtime `VARIANT`.

Purpose: eliminate `typeof()` + `as` branching on values that may be one of a bounded set of types (e.g. physics shape `size` being `float` or `Vector3`).

### `@private` Annotation

Syntax: `@private var x`, `@private func f()`, `@private const K`, `@private class Inner`

- Parser registers the annotation; sets `is_private` on `VariableNode` / `FunctionNode` / `ConstantNode` / `ClassNode`.
- Analyzer blocks external access (`Cannot access private member "X" of class "Y"`), including calls to private methods; a blocked access reports one clean error (no cascading "cannot find member").
- Access policy: private members are visible to the declaring class and to **any class in the same script** (nested classes, any depth, including siblings). Other scripts are blocked.
- Autocomplete filters private members of other classes (`p_recursion_depth > 0` check in `_find_identifiers_in_class`).
- `@private` cannot be combined with any `@export*` annotation (error in both orders).
- Name reuse: a `@private` member still occupies the name in subclasses (upstream conflict check applies: `The member "X" already exists in parent class`). Deliberate decision (2026-08-12): supporting shadowing requires separate storage slots, which makes `GDScript::member_indices` sparse and forces an O(n) scan on the instance-creation hot path (plus ABI-safe persistence of the slot count is not possible without changing `gdscript.h`). Not worth the cost until a real need appears.

Purpose: encapsulation for internal members without a visibility keyword in the language.

Implementation note: `gdscript.h` must stay layout-identical to upstream: `main/main.cpp` and `editor/doc/editor_help.cpp` include it outside the module and would break ABI on any layout change (verified the hard way — heap corruption). Field additions require a header-redirect design first.

### String Constructors

Syntax: `String(int)`, `String(float)`, `String(bool)` → makes `1 as String` produce `"1"`.

- Implemented via `VariantConstructorToString<T>` in the goblin `core/variant/variant_construct.{cpp,h}` (overridden through the core file override, ADR 0001).

### Shaped Dictionary Literals

Syntax: typed entries, Lua style — `key: Type = value` (default optional):

```gdscript
var tpl := {
    hp: int = 10,
    reach: float = 1.5,
    tags: Array[String] = [],
}
```

- Parser: typed entries in `{}` literals; recursive shape = key set + entry types (`GDScriptDataType` shape payload in `gdscript_function.h`).
- Analyzer: shape inference; the shape is preserved across all declaration styles (`:=`, `: Dictionary`, `: Dictionary[K,V]`, and untyped `=`), and writes to typed keys are compile-time errors; attribute access (`dict.key`) and constant-index access (`dict["key"]`) refine to the entry type; unknown keys fall back to the flat value type (or `Variant`) — deliberate, so shaped dicts stay extensible like plain dictionaries; autocomplete recurses into shapes (`gdscript_editor.cpp`).
- Runtime: `OPCODE_CONSTRUCT_SHAPED_DICTIONARY` validates every literal value against its declared entry type (debug safety net) and normalizes typed containers (plain `Array` → typed `Array[T]`); the recursive datatype travels as raw instruction words (`append_datatype()`, `gdscript_byte_codegen.h`), decoded by `GDScriptFunction::decode_datatype()` (`gdscript_function.{h,cpp}`). Runtime validation applies at construction only — later writes are enforced at compile time, not re-checked at runtime. For `: Dictionary[K,V]` declarations the constructed dictionary is typed as declared (`set_typed` + per-entry `set()`, so flat key/value types are enforced in all builds), while entries still normalize to the per-key shape.
- Style rules: typed entries are Lua style only; mixing with Python-style untyped literals errors (tests: `shaped_dictionary_style_mixing_*`, `shaped_dictionary_typed_in_python`).
- **`@schema`** (implemented 2026-08-19, G-18; hardened 2026-08-20): a `const` Dictionary annotated `@schema` becomes a project-wide reusable **schema**; `Dictionary[Name]` instantiates it. Per-key defaults autofill at construction (`var m: Dictionary[critter]` yields `hp=10, name="", ...` without any initializer, for locals, members and the implicit initializer); override literals merge (`= { hp = 20 }` keeps other defaults) and are type-checked against the schema at compile time (wrong type = error); unknown keys grow the dictionary (Variant, G-17 rule — no fixed/strict mode). Rules: `@schema` requires a class-level constant (local use = parse error) + a shaped dictionary literal (untyped literal = error); a non-schema const in `Dictionary[...]` = error; `Dictionary[Name]` is a *type annotation only* — using it as an expression (`var x = Dictionary[Car]`) is an analyzer error (`Cannot use type "Dictionary" as a value...`, shared guard rejects `Array[int]`-style type-name subscripts in expression position too). Engine surface: `GDScriptParser::DataType` + `GDScriptDataType` carry per-key defaults (`dictionary_shape_defaults`) + `is_schema`/`schema_name`; the schema's datatype is reused via const-as-type in `resolve_datatype` (local / class member / global registry branches); the **global schema registry** (`GDScriptLanguage::schemas`, name → declaring script path) is source-based and populated from three sources, class_name-style: the editor's class-name scan (`_get_global_class_name` body-parses files containing `@schema` and registers the names), `GDScript::reload` (re-synced from the parse tree immediately after parse, *before* analysis, so `Dictionary[Name]` resolves even while the declaring script's own analysis is in flight through an `extends` chain; removal only on parse failure), and a persisted cache (`res://.godot/goblin_schema_cache.cfg`) that is eagerly loaded at `GDScriptLanguage::init()` (before any script analysis), saved at the scan/reload registration points, written once as an empty file on the first run after an upgrade while invalidating the editor's `filesystem_cache` (forces the full scan that seeds the registry), and shipped in exports by the gdscript export plugin. Cross-file `Dictionary[Name]` resolves the declaring script on demand through the parser cache. Defaults are serialized in `append_datatype`/`decode_datatype` (constant-table refs) and filled by `OPCODE_CONSTRUCT_SHAPED_DICTIONARY` (defaults first, literal entries override; container defaults deep-copied so instances own mutable values; typed-container defaults normalize). Schema constants stay read-only dictionaries of defaults. Known v1 limits: a project never opened in the editor (no cache file) run directly in game mode still needs the declaring script loaded first (load-order, class_name parity); inner-class schemas resolve only within their own script; `Dictionary[Name]` not wired into autocomplete suggestions.

Purpose: typed dictionaries with zero runtime lookups for data-driven entity templates — the language-layer answer to the genre set's dict-heavy entity model.

### `then` / `elthen` (safe navigation / null coalescing)

Keywords `then` and `elthen` are the fork's syntax. `?.` / `??` are NOT planned.

State (verified): fully implemented — tokenizer (`Token::THEN`/`Token::ELTHEN`, `gdscript_tokenizer.h:65-66`), parser (`PREC_NULLISH` precedence, `OP_SAFE_NAVIGATE`/`OP_NULL_COALESCE`, `gdscript_parser.{h,cpp}`), analyzer (short-circuit typing + constant folding, `gdscript_analyzer.cpp` `reduce_binary_op`), compiler (ternary-based codegen, `gdscript_compiler.cpp`). No VM changes.

Semantics locked 2026-08-13 (deliberate; differs from the earlier null-only recommendation in `modules/goblin/docs/rfc/native-game-features-rfc.md` §3.2, which described gdscript2's runtime truthiness as a wart):

- `a then b` → `a != null ? b : a` — **null-only** safe navigation; chainable (`a then b then c`).
- `a elthen b` → `a ? a : b` — **truthiness** coalescing: `0 elthen 5` → `5`, `"" elthen "x"` → `"x"`, `false elthen 1` → `1`.
- Caveat: `elthen`'s static result type is the left operand's type whenever it is non-nil, but a falsy left yields the right operand's value at runtime — the static type can be broader than the actual value (`var x: int = 0 elthen "s"` compiles, evaluates to `"s"`).
- Tests: pending — see TD-02 in `backlog.md`.

## Divergence Surface

When porting to a new stable release, review these files for merge conflicts:

| Area | Files |
|------|-------|
| Tokenizer | `gdscript_tokenizer.h`, `gdscript_tokenizer.cpp`, `gdscript_tokenizer_buffer.{h,cpp}` |
| Parser / AST | `gdscript_parser.h`, `gdscript_parser.cpp` |
| Analyzer | `gdscript_analyzer.h`, `gdscript_analyzer.cpp` |
| Compiler | `gdscript_compiler.cpp` |
| Bytecode gen | `gdscript_byte_codegen.{h,cpp}`, `gdscript_codegen.h` |
| VM | `gdscript_vm.cpp` |
| Function | `gdscript_function.{h,cpp}` |
| Editor (autocomplete) | `gdscript_editor.cpp` |
| Core variant | `core/variant/variant_construct.cpp`, `core/variant/variant_construct.h` |

## Planned Features

See [backlog.md](backlog.md) §1. Next priorities: `then`/`elthen` tests (TD-02), structs (G-07), typed dictionaries (G-08).
