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

    def goblin_add_library(self_env, program, source, **kw):
        program_str = str(program)
        if program_str.startswith("#bin/godot"):
            program = program_str.replace("godot", "goblin")
            if self_env.get("verbose"):
                print(f"Goblin: Renaming {program_str} -> {program}")
        return godot_methods.add_library(self_env, program, source, **kw)

    def goblin_add_shared_library(self_env, program, source, **kw):
        program_str = str(program)
        if program_str.startswith("#bin/godot"):
            program = program_str.replace("godot", "goblin")
            if self_env.get("verbose"):
                print(f"Goblin: Renaming {program_str} -> {program}")
        return godot_methods.add_shared_library(self_env, program, source, **kw)

    env.AddMethod(goblin_add_program, "add_program")
    env.AddMethod(goblin_add_library, "add_library")
    env.AddMethod(goblin_add_shared_library, "add_shared_library")

    env.Append(CPPDEFINES=["GOBLIN_ENGINE"])

    if env.get("verbose"):
        print("Goblin: Build hooks enabled (builders monkey-patched)")



def get_doc_classes():
    return []


def get_doc_path():
    return "doc_classes"
