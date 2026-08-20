# Genre Coverage â€” Needs Ã— Godot Ã— Fork Plan

Date: 2026-08-15. Analysis of every genre in the vision against what Godot 4.7.x offers natively and what the fork must provide. Companion to `ROADMAP.md` Â§1 (vision) and `backlog.md`.

Legend: **overlap** = Godot covers it (scriptable where noted) Â· **gap â†’ item** = fork plan item (backlog ID).

---

## 1. FPS

| Need | Godot offers | Godot lacks â†’ fork |
|---|---|---|
| Movement (walk/sprint/crouch/jump/slide) | CharacterBody3D + `move_and_slide`, FOV, mouse capture | â€” scriptable |
| Camera (headbob, shake, ADS) | Camera3D, Tween, AnimationPlayer | â€” scriptable |
| Hitscan weapons | RayCast3D, `intersect_ray`, shape queries | **Impact surface metadata** (class, UV, material) â†’ M-09 |
| Projectiles | RigidBody3D, particles | â€” scriptable |
| Damage/health/location | Area3D hitboxes | No native framework â†’ scriptable (title-side) |
| Decals, tracers, muzzle flash | Decal node, GPUParticles3D | â€” scriptable |
| Enemy AI | NavigationAgent3D/Region3D/Obstacle3D, RVO avoidance | Cover AI â†’ scriptable |
| Many enemies perf | â€” | **GDScript hot paths** â†’ G-10/G-11 |

**Verdict:** Godot covers ~90% of FPS mechanics. One real engine gap (hitscan metadata), one perf gap (planned).

## 2. RPG

| Need | Godot offers | Godot lacks â†’ fork |
|---|---|---|
| Items/quests/dialogue data | Resource system, JSON, ConfigFile | No data-table framework â†’ **G-08 typed dicts, G-07 structs**, with **G-18 (template dicts)** spec locked 2026-08-19 (`@schema` const dictionaries, RFC Â§2.0), no `core/variant` change |
| Inventory/containers/shops | Full Control UI toolkit | No native system â†’ scriptable (title-side) |
| Stats/levels/skill trees | â€” | Scriptable |
| Dialogue | Dialogic (community addon) | Scriptable (title-side) |
| Quests/objectives | Signals, groups | Scriptable (title-side) |
| **Entity model as data** | Dictionary is untyped, copy-by-ref | **G-07 structs** (kills 60+ `duplicate(true)`), typed containers |
| Save/load world state | ResourceSaver, manual serialization | No systemic save framework â†’ title-side delta save/load (adopted) |
| Rules/condition eval | â€” | Scriptable (title-side Rule) â€” C++ reaction server correctly rejected |

**Verdict:** RPG is a *data-layer* genre. Godot's language gap (untyped dicts, no value types) is the whole story â†’ G-07/G-08, with G-18 (template dicts) spec locked 2026-08-19 (`@schema` const dictionaries, RFC Â§2.0), GDScript-side registry pattern proven by `data.gd`; TD-04 gates any future `core/variant` perf change.

## 3. Shooter (general/TPS/arena)

| Need | Godot offers | Godot lacks â†’ fork |
|---|---|---|
| Third-person camera | SpringArm3D (native!) | â€” |
| Networking (if multiplayer) | High-level multiplayer, ENet/WebSocket/WebRTC modules (kept) | â€” |
| Everything else | Same as FPS | Same as FPS |

**Verdict:** Fully covered. No fork work beyond FPS items.

## 4. Boomer Shooter

| Need | Godot offers | Godot lacks â†’ fork |
|---|---|---|
| Static baked lighting | LightmapGI + LightmapperRD (editor-only) | **Runtime CPU baking on GLES3** â†’ C-02 `lightmapper_cpu`; culling bug fix C-01; editor pipeline C-11 |
| **Lightstyles** (animated baked light flicker) | â€” | Nothing â†’ **M-11** |
| **Palette/indexed color** | â€” | Nothing â†’ **M-06, M-12** |
| Texture animation / color cycling | Shader TIME (scriptable) | Editor-friendly nodes â†’ M-13 |
| Pixel-perfect scaling | Nearest/integer stretch | **CUT1/2 upscalers (DONE)** |
| **Kinematic movers** (doors/lifts/crushers) | AnimatableBody3D (carries player) | Blocking policy, crush damage, deterministic order â†’ **M-10** |
| Hitscan metadata | â€” | Same as FPS â†’ M-09 |
| Level geometry | Scene-based (no BSP â€” fine) | Title converts sectors â†’ ArrayMesh |
| PVS/occlusion | Occluder3D + baked occlusion | â€” covered |
| Dither/CRT/post | Canvas shaders | Scriptable (title-side) |

**Verdict:** Boomer shooter is where the *retro presentation* gaps concentrate â€” lightstyles, palettes, texture animation â€” plus the mover contract. All planned, all currently P3.

## 5. Immersive Sim

| Need | Godot offers | Godot lacks â†’ fork |
|---|---|---|
| Physics manipulation (pickup/throw/stack) | RigidBody3D, joints, impulses â€” **strong** | â€” |
| **Interaction** (verbs, prompts, focus) | Area3D + input_pickable (raw) | No interaction contract â†’ title-side scripted; no migration case yet |
| **Light perception / AI visibility** | Raycasts only | **No light field** â†’ C-05/M-07 + M-08 stealth value (Thief light gem) |
| **Audio occlusion/propagation** | AudioStreamPlayer3D + reverb bus | No occlusion, no portal re-emission â†’ **C-06** |
| Lock/key, doors, levers | Area3D + scripts | Mover semantics â†’ M-10 |
| Portals/spatial weirdness | SubViewport + teleport (manual); Portal/Room nodes are culling-only | Real traversal portals â†’ **M-05** |
| NPC schedules/cadence | `_process`, SceneTreeTimer | Native cadence â†’ title-side scheduler (adopted; native rejected) |
| Systemic state save | â€” | Title-side delta save/load (adopted) |

**Verdict:** Immersive sim is the fork's *primary focus* â€” its three genuine gaps (perception fields, audio occlusion, portals) are all planned, but perception + occlusion sit at P2 and portals at P3. **This is the cluster where priority is wrong.**

## 6. Systemic Game

| Need | Godot offers | Godot lacks â†’ fork |
|---|---|---|
| Events/stimulus | Signals, groups, autoloads | No global bus â€” signals + title-side queue suffice (reaction server rejected correctly) |
| Data-driven rules | Resources + GDScript | No rule engine â€” title-side; language layer helps (G-19 callable shorthand) |
| **Entity data model** | Nodes | Untyped dicts â†’ **G-17 (DONE), G-08, G-07, G-18 (locked: `@schema`)** |
| Perception fields (light/acoustics) | â€” | â†’ C-05/M-07/M-08, C-06 |
| Tick/cadence scheduling | `process_mode`, custom groups via script | Native cadence rejected; title-side scheduler |
| Determinism | Per-version, no guarantees | Not planned (no rollback need) |
| Procedural content | noise module (kept) | â€” |
| Simulation glue (timers, tweens) | SceneTreeTimer, Tween â€” **native** | â€” |

**Verdict:** Godot's glue (signals/timers/tweens) covers the plumbing. The genuine systemic gaps are the **data layer** (planned, P1) and **perception fields** (planned, P2).

## 7. Low-Fi/Retro (presentation)

| Need | Godot offers | Godot lacks â†’ fork |
|---|---|---|
| GL Compatibility renderer | GLES3 â€” **native** (kept, primary target) | â€” |
| Pixel-art upscaling | Nearest/integer scaling | **CUT1/2 (DONE)** |
| Palettes / indexed color | â€” | â†’ M-06/M-12 |
| Dithering, posterization | Canvas shaders | Scriptable / M-06 editor nodes |
| Color cycling, texture anim | Shader TIME | â†’ M-13 |
| **Low-fi audio (chiptune/MIDI)** | WAV only | **MIDI module (DONE)** â€” TinySoundFont, importers |
| Low-res rendering | Viewport scaling, SubViewport â€” native | â€” |
| Fog, retro atmosphere | Environment fog â€” native | â€” |
| Lightmaps | LightmapGI (RD, editor) | Runtime CPU bake â†’ C-02 |

**Verdict:** Godot natively covers most retro *presentation plumbing*; the true gaps are palettes + color-cycling nodes (planned, P3) and the fork already shipped the two biggest retro wins (CUT, MIDI).

---

# Synthesis

Godot covers the mechanics layer almost completely â€” movement, physics sandbox, UI, audio, animation, navigation, occlusion, viewport scaling, networking. That overlap is why a *soft fork* is right: don't rebuild what Godot does well.

The genuine gaps cluster in exactly 4 places, and the plan already targets all 4:

| Cluster | Items | Plan status |
|---|---|---|
| 1. **Language data layer** (systemic/RPG core) | G-17 done Â· G-18 (locked: `@schema`) Â· G-07, G-08 | âœ… P1, correctly prioritized Â· **Updated 2026-08-19:** G-18 spec locked â€” `@schema` const dictionaries (defaults autofill, typed overrides, growable beyond schema; `Dictionary[Name]` instantiation; engine surface per RFC Â§2.0 â€” const-as-type + G-17 shape fields, defaults vector, VM default-fill, global schema registry; no `core/variant` change). data.gd proves the GDScript registry pattern; TD-04 gates future `core/variant` perf; G-07/G-08 live language-data items |
| 2. **Perception/simulation fields** (immersive sim + systemic) | C-05/C-06/M-07/M-08 folded â†’ **SimServer S-01/S-02/S-03** (`docs/rfc/simserver-rfc.md`, 2026-08-16) | âœ… P1 â€” now the primary-focus pillar with real engine items |
| 3. **Retro presentation** (boomer + low-fi) | CUT done Â· M-06/M-11/M-12/M-13 | âš ï¸ P3 â€” retro is secondary by charter |
| 4. **Genre contracts** (hitscan metadata â†’ S-02, movers, portals) | M-09 folded â†’ S-02 Â· M-10/M-05 | âš ï¸ P3 â€” title has scripted substitutes |

**Conclusion:** nothing is missing from the plan. The only real question is priority: **cluster 2 (perception fields) deserves P1** â€” it is the one cluster where the primary focus has zero delivered substance, and it has no title-side substitute (the viewport-based light sensor is explicitly not scalable). **Resolved 2026-08-16:** cluster 2 is now SimServer (S-01/S-02/S-03, P1) per `docs/rfc/simserver-rfc.md`. Clusters 3â€“4 stay P3 honestly: the title's scripted solutions work today and retro is secondary.

Recommended adjustments (2026-08-15 alignment review):

1. Charter wording: narrow the "Systemic out of the box" promise to what is actually built (perception fields, occlusion, data layer); interaction/cadence explicitly title-side until a migration case exists. **Resolved 2026-08-16:** no narrowing needed â€” SimServer gives interaction/cadence/stimulus/fields real engine items (S-01â€¦S-04).
2. Promote C-05/M-07/M-08 (spatial field + stealth value) to P1 after G-18. **Resolved 2026-08-16:** folded into SimServer S-01/S-03, P1. **Note 2026-08-19:** cluster 2 is independently P1 (S-01/S-02/S-03 shipped); G-18 is a cluster-1 item and does not gate cluster 2.
3. Start G-07 structs de-risk (50 parser-only test cases) â€” pure analysis, unblocks the biggest-gap decision. **Grounded 2026-08-19** by `data.gd`: the reference title's `Data` entity model is dict-heavy with 8+ `duplicate(true)` sites and no value/copy-by-value semantics anywhere â€” structs are the live language-data gap.
4. Schedule palettes (M-06, 2â€“3d) or drop the word from the charter.
5. Perf validation gate for G-10/G-11 â€” profile the reference title first, port only the winning opcodes.
6. P3 hygiene: each P3 item gets a "build if / kill if" line; networking + platform keeps recorded as user-directed exceptions in the charter.
