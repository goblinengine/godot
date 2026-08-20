# ===================================================================
# MODULE TRIM — 26 modules disabled (~51% faster compile)
# Mechanism: build-time option injection (ADR 0012). module_*_enabled
# is decided at SConstruct:499 opts.Update from ARGUMENTS (args layer
# beats defaults and custom.py/profile files). configure() runs AFTER
# that Update (gate loop, SConstruct:1124) — too late for the trim
# candidates sorting before "goblin". So the injection happens at
# IMPORT time (first module loop, SConstruct:474), before the Update:
# for each trimmed module without a user CLI value, ARGUMENTS gets
# module_<name>_enabled = "no". The gate at SConstruct:1113 then skips
# all of them regardless of alphabetical position. User CLI wins.
# Precedent: methods.py:229, platform/android/detect.py:118.
# ===================================================================
DISABLE_MODULES = {
     # Image/texture formats (PNG only) — tinyexr KEPT (editor .exr
     # lightmap save, Image::save_exr needs modules/tinyexr; C-11)
     # NOTE: astcenc is NOT trimmed — the pre-built ANGLE static library
     # (godotengine/godot-angle-static) bundles an AstcDecompressor that
     # links against astcenc_* symbols (astcenc_context_alloc, etc.).
     # Trimming it causes LNK2019 unresolved externals on Windows builds
     # that use ANGLE. astcenc is decoder-only in template_release
     # (ASTCENC_DECOMPRESS_ONLY), minimal code footprint.
     # NOTE: webp + jpg are NOT trimmed (2026-08-18): vanilla Godot 4.7
     # writes lossless/lossy texture imports as WebP-embedded .ctex
     # (DATA_FORMAT_WEBP), so trimming webp makes every pre-existing
     # vanilla-imported project cache unreadable (compressed_texture.cpp:343
     # "img.is_null() || img->is_empty()"). jpg has the same failure mode
     # for .jpg source textures. Kept for Godot compatibility.
     "bmp", "tga", "dds", "hdr",
     "basis_universal", "ktx", "etcpak",
     # Audio/video (no video playback, no interactive music)
     "theora", "interactive_music",
    # VR/XR (no VR usage)
    "webxr", "openxr", "mobile_vr",
    # 3D nodes/scene (no CSG, GridMap, GLTF/FBX import)
    "csg", "gridmap", "gltf", "fbx",
    # Navigation (AStar3D from core/math, not these modules)
    "navigation_3d", "navigation_2d",
    # 2D physics KEPT since 2026-08-16 (user directive): the fork ships the
    # real 2D physics server, not the dummy fallback. godot_physics_3d also
    # KEPT: jolt_physics registers "Jolt Physics" but NOT the default
    # (jolt_physics/register_types.cpp:59); GodotPhysics3D is the registered
    # default (godot_physics_3d/register_types.cpp:56-57) used by
    # main.cpp:362-372 fallback and tests/test_main.cpp:204
    # new_default_server(). Trimming it would drop the test suite +
    # fresh-project boot to the dummy server.
    # Shader compiler (GL Compatibility uses GLSL directly, no SPIR-V;
    # RenderingDevice shader_compile_spirv_from_source fails without
    # glslang — Forward+/Mobile unsupported by design, ADR 0003)
    "glslang",
    # Utility (no webcam, no runtime zip)
    "camera", "zip",
    # Rendering (Embree occlusion culling — Forward+/Mobile only)
    "raycast",
    # Debug/profiling (release builds only)
    "objectdb_profiler",
    # Physics tools (convex decomposition, editor only)
    "vhacd",
}

# Import-time injection: runs when this module's config.py is imported
# in the first module loop (SConstruct:474), before the options Update
# at SConstruct:499. Idempotent across the second (gate-loop) import.
from SCons.Script import ARGUMENTS
for _mod in DISABLE_MODULES:
    _key = f"module_{_mod}_enabled"
    if _key not in ARGUMENTS:
        ARGUMENTS[_key] = "no"


def can_build(env, platform):
    return True


def configure(env):
    """
    SURGICAL ENVIRONMENT INTERCEPTION
    
    Hook into the build system early to inject Goblin branding.
    This runs before any SCsub files, so we can monkey-patch builders.
    """
    import os
    import sys
    
    # Import core builders to wrap them
    goblin_path = os.path.dirname(__file__)
    core_path = os.path.join(goblin_path, "..", "..", "core")
    sys.path.insert(0, core_path)
    import core_builders
    sys.path.pop(0)
    
    # Import main builders to wrap them
    main_path = os.path.join(goblin_path, "..", "..", "main")
    sys.path.insert(0, main_path)
    import main_builders
    sys.path.pop(0)
    
    # Store original builders FIRST before importing goblin_builders
    _original_version_builder = core_builders.version_info_builder
    _original_authors_builder = core_builders.make_authors_header
    _original_donors_builder = core_builders.make_donors_header
    _original_license_builder = core_builders.make_license_header
    _original_splash_builder = main_builders.make_splash
    _original_app_icon_builder = main_builders.make_app_icon
    if hasattr(main_builders, 'make_splash_editor'):
        _original_splash_editor_builder = main_builders.make_splash_editor
    
    # NOW import goblin builders (they will see the original builders)
    sys.path.insert(0, goblin_path)
    import goblin_builders
    sys.path.pop(0)
    
    # Make original builders available to goblin_builders
    goblin_builders._original_core_builders = {
        'version_info_builder': _original_version_builder,
        'make_authors_header': _original_authors_builder,
        'make_donors_header': _original_donors_builder,
        'make_license_header': _original_license_builder,
    }
    goblin_builders._original_main_builders = {
        'make_splash': _original_splash_builder,
        'make_app_icon': _original_app_icon_builder,
    }
    if hasattr(main_builders, 'make_splash_editor'):
        goblin_builders._original_main_builders['make_splash_editor'] = _original_splash_editor_builder
    
    # Replace with Goblin versions
    core_builders.version_info_builder = goblin_builders.goblin_version_info_builder
    core_builders.make_authors_header = goblin_builders.goblin_authors_builder
    core_builders.make_donors_header = goblin_builders.goblin_donors_builder
    core_builders.make_license_header = goblin_builders.goblin_license_builder
    main_builders.make_splash = goblin_builders.goblin_splash_builder
    main_builders.make_app_icon = goblin_builders.goblin_app_icon_builder
    
    # Handle editor splash if it exists
    if hasattr(main_builders, 'make_splash_editor'):
        main_builders.make_splash_editor = goblin_builders.goblin_splash_editor_builder
    
    # Rename binaries from godot to goblin
    import methods as godot_methods

    def goblin_add_program(self_env, program, source, **kw):
        program_str = str(program)
        if program_str.startswith("#bin/godot") and "console" not in program_str.lower():
            program = program_str.replace("godot", "goblin")
            if self_env.get("verbose"):
                print(f"Goblin: Renaming {program_str} -> {program}")
        return godot_methods.add_program(self_env, program, source, **kw)

    # Single-file overrides per library: {library_name: {source_stem: goblin_path}}.
    # Library names are the plain strings passed to add_library() ("core", "editor").
    # Goblin copies must NOT be globbed by any SCsub (modules/goblin/editor/SCsub
    # globs *.cpp non-recursively) — keep them in subdirectories like overrides/.
    _goblin_dir = os.path.dirname(__file__)
    _GOBLIN_FILE_OVERRIDES = {
        "core": {
            "variant_construct": os.path.join(_goblin_dir, "core", "variant", "variant_construct.cpp"),
        },
        "editor": {
            "editor_about": os.path.join(_goblin_dir, "editor", "overrides", "gui", "editor_about.cpp"),
            "project_export": os.path.join(_goblin_dir, "editor", "overrides", "export", "project_export.cpp"),
            "project_manager": os.path.join(_goblin_dir, "editor", "overrides", "project_manager", "project_manager.cpp"),
            "editor_node": os.path.join(_goblin_dir, "editor", "overrides", "editor_node.cpp"),
        },
        "drivers": {
            "post_effects": os.path.join(_goblin_dir, "drivers", "gles3", "effects", "post_effects.cpp"),
            "rasterizer_scene_gles3": os.path.join(_goblin_dir, "drivers", "gles3", "rasterizer_scene_gles3.cpp"),
            "render_scene_buffers_gles3": os.path.join(_goblin_dir, "drivers", "gles3", "storage", "render_scene_buffers_gles3.cpp"),
        },
        "servers": {
            "renderer_viewport": os.path.join(_goblin_dir, "servers", "rendering", "renderer_viewport.cpp"),
        },
        "scene": {
            "viewport": os.path.join(_goblin_dir, "scene", "main", "viewport.cpp"),
            # Fast scene tree (M-14, direction 2026-08-17): SceneTree is modified
            # IN PLACE — this mirror is the edit home. Content is a faithful copy
            # of upstream scene_tree.cpp; optimizations land here directly (no
            # module, no BaseSceneTree seam).
            "scene_tree": os.path.join(_goblin_dir, "scene", "main", "scene_tree.cpp"),
        },
    }

    # New sources ADDED to a library (no upstream counterpart — the override
    # dict above only replaces existing stems; these have no stem to replace).
    # EntityNode/EntityComponent (D-20): compiled into the scene library so the
    # mirror scene_tree.cpp (same library, overlay env) can include and drive
    # the registry. Registered at MODULE_INITIALIZATION_LEVEL_SCENE from
    # register_types.cpp (precedent: modules/sim/register_types.cpp).
    _GOBLIN_FILE_ADDITIONS = {
        "scene": {
            "entity_registry": os.path.join(_goblin_dir, "scene", "main", "entity_registry.cpp"),
            "entity_node": os.path.join(_goblin_dir, "scene", "main", "entity_node.cpp"),
            "entity_component": os.path.join(_goblin_dir, "scene", "main", "entity_component.cpp"),
            "transform_3d_component": os.path.join(_goblin_dir, "scene", "main", "transform_3d_component.cpp"),
            "mesh_instance_component": os.path.join(_goblin_dir, "scene", "main", "mesh_instance_component.cpp"),
            "visibility_component": os.path.join(_goblin_dir, "scene", "main", "visibility_component.cpp"),
        },
    }

    def goblin_add_library(self_env, program, source, **kw):
        program_str = str(program)
        if program_str.startswith("#bin/godot"):
            program = program_str.replace("godot", "goblin")
        # Replace selected sources in the core/editor libraries before the library is created.
        lib_name = str(program).replace("#bin/obj/", "").split(".", 1)[0]

        # Goblin mirrors compile with the goblin tree as a pseudo-root include
        # overlay: root-relative includes ("drivers/gles3/effects/post_effects.h")
        # resolve to the goblin mirror first, then fall through to upstream.
        _goblin_env = self_env.Clone()
        _goblin_env.Prepend(CPPPATH=[_goblin_dir])

        overrides = _GOBLIN_FILE_OVERRIDES.get(lib_name)
        if overrides:
            _new_source = []
            for _s in source:
                # Sources are Object nodes whose str() is the target path, e.g.
                # "#bin/obj/editor/gui/editor_about.windows.editor.x86_64.obj".
                # The stem is everything before the first dot of the basename.
                _stem = os.path.basename(str(_s)).split(".", 1)[0]
                if _stem in overrides and os.path.isfile(overrides[_stem]):
                    if self_env.get("verbose"):
                        print(f"Goblin: Overriding {_stem} -> {overrides[_stem]}")
                    _new_source.append(_goblin_env.Object(overrides[_stem]))
                else:
                    _new_source.append(_s)
            source = _new_source
        # New sources ADDED to the library (no upstream stem to replace).
        additions = _GOBLIN_FILE_ADDITIONS.get(lib_name)
        if additions:
            _added = []
            for _stem, _path in additions.items():
                if os.path.isfile(_path):
                    if self_env.get("verbose"):
                        print(f"Goblin: Adding {_stem} -> {_path}")
                    _added.append(_goblin_env.Object(_path))
            if _added:
                source = list(source) + _added
        return godot_methods.add_library(self_env, program, source, **kw)

    def goblin_add_shared_library(self_env, program, source, **kw):
        program_str = str(program)
        if program_str.startswith("#bin/godot"):
            program = program_str.replace("godot", "goblin")
            if self_env.get("verbose"):
                print(f"Goblin: Renaming {program_str} -> {program}")
        return godot_methods.add_shared_library(self_env, program, source, **kw)

    # ------------------------------------------------------------------
    # Windows exe icon: swap the .rc source so the goblin icon + version
    # info land in the binary. platform/windows/SCsub compiles
    # godot_res.rc (icon: godot.ico); we redirect to our goblin.rc and
    # generate goblin.ico from app_icon.png at build time.
    # ------------------------------------------------------------------
    _GOBLIN_WIN_RC = os.path.join(_goblin_dir, "platform", "windows", "goblin.rc")
    _GOBLIN_WIN_RC_WRAP = os.path.join(_goblin_dir, "platform", "windows", "goblin_res_wrap.rc")
    _GOBLIN_ICO = os.path.join(_goblin_dir, "platform", "windows", "goblin.ico")
    _GOBLIN_APP_ICON = os.path.join(_goblin_dir, "main", "app_icon.png")

    if env["platform"] == "windows":
        _orig_res_builder = env["BUILDERS"]["RES"]

        # Generate goblin.ico from app_icon.png at build time. Created here
        # (configure runs before every SCsub) so the RES wrapper below can
        # declare the dependency before platform/windows/SCsub invokes it.
        _goblin_ico_node = env.CommandNoCache(
            _GOBLIN_ICO,
            _GOBLIN_APP_ICON,
            env.Run(goblin_builders.goblin_ico_builder),
        )

        def goblin_res_builder(env, target, source, **kwargs):
            # env.RES(...) used to be a BuilderWrapper (which massages args into
            # lists); our AddMethod shadow receives raw args, so normalize here.
            if target is not None and not isinstance(target, list):
                target = [target]
            if source is not None and not isinstance(source, list):
                source = [source]
            _new_source = []
            for _s in source:
                _sn = str(_s)
                if "godot_res.rc" in _sn or "godot_res_template.rc" in _sn:
                    _new_source.append(env.File(_GOBLIN_WIN_RC))
                elif "godot_res_wrap.rc" in _sn or "godot_res_wrap_template.rc" in _sn:
                    _new_source.append(env.File(_GOBLIN_WIN_RC_WRAP))
                else:
                    _new_source.append(_s)
            # Builder.__call__(env, target, source, **kw)
            _result = _orig_res_builder(env, target, _new_source, **kwargs)
            # rc.exe reads goblin.ico at action time; make SCons build it first.
            env.Depends(_result, _goblin_ico_node)
            return _result

        env.AddMethod(goblin_res_builder, "RES")

    env.AddMethod(goblin_add_program, "add_program")
    env.AddMethod(goblin_add_library, "add_library")
    env.AddMethod(goblin_add_shared_library, "add_shared_library")

    env.Append(CPPDEFINES=["GOBLIN_ENGINE"])

    # ===================================================================
    # EDITOR SPLASH — upstream 4.7 removed it (commit c283fce698:
    # "Remove editor splash screen with sponsors logo"). SConstruct:283
    # defaults no_editor_splash=True and :587-591 forces it + appends
    # NO_EDITOR_SPLASH because #main/splash_editor.png no longer exists.
    # We keep the flag True so main/SCsub skips its own (broken) command,
    # drop the define, and generate splash_editor.gen.h ourselves from
    # modules/goblin/main/splash_editor.png (see modules/goblin/SCsub).
    # ===================================================================
    if env.editor_build:
        try:
            env["CPPDEFINES"].remove("NO_EDITOR_SPLASH")
        except ValueError:
            pass

    # ===================================================================
    # MODULE TRIM CANARY — the gate is env["module_<name>_enabled"],
    # injected at import time above (ADR 0012). By now (configure,
    # gate loop) SConstruct:499 has already applied ARGUMENTS, so every
    # trimmed module must read False. Count < len(DISABLE_MODULES) means
    # the injection regressed (upstream options-flow drift) or the user
    # re-enabled modules via CLI — both visible in every build log.
    # ===================================================================
    _disabled = [m for m in DISABLE_MODULES if not env[f"module_{m}_enabled"]]
    print(f"Goblin: Module trim active ({len(_disabled)}/{len(DISABLE_MODULES)} modules gated off)")

    if env.get("verbose"):
        print("Goblin: Build hooks enabled (builders monkey-patched)")



def get_doc_classes():
    # EntityNode/EntityComponent hybrid tree+ECS layer (D-20).
    return [
        "EntityNode",
        "EntityComponent",
        "Transform3DComponent",
        "MeshInstanceComponent",
        "VisibilityComponent",
    ]


def get_doc_path():
    return "doc_classes"


def get_icons_path():
    # Editor icon overrides (Logo.svg, Godot.svg, TitleBarLogo.svg, ...) must be
    # registered at configure time: SConstruct collects module_icons_paths BEFORE
    # editor/icons/SCsub generates editor_icons.gen.h. Registering from this
    # module's editor/SCsub would run too late and the overrides would never apply.
    return "editor/icons"
