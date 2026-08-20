"""Functions used to rebrand Godot to Goblin during build time"""

import importlib.util
import os
import struct
import sys
from collections import OrderedDict

# Import core builders to reuse their logic
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "core"))
import core_builders
sys.path.pop(0)

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
import methods
sys.path.pop(0)

# These will be set by config.py before builders are called
_original_core_builders = {}
_original_main_builders = {}


def goblin_version_info_builder(target, source, env):
    """Override version info with Goblin branding"""
    # Source[0] is a Value() node containing version.version_info dict from version.py
    # Source[1] is the version_override.py file (if present)

    # Get base version info from SCons Value node
    base_version = source[0].read() if hasattr(source[0], 'read') else {}
    if not isinstance(base_version, dict):
        base_version = {}

    # Load Goblin overrides from modules/goblin/core/version_override.py
    override_path = str(source[1]) if len(source) > 1 else os.path.join(os.path.dirname(__file__), "core", "version_override.py")
    override_data = {}
    if os.path.exists(override_path):
        spec = importlib.util.spec_from_file_location("goblin_version_override", override_path)
        override_mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(override_mod)
        override_data = getattr(override_mod, "goblin_version", {})
        if not isinstance(override_data, dict):
            override_data = {}

    version_data = {
        # Numeric fields fall back to zero if the upstream dict is missing
        "major": base_version.get("major", 0),
        "minor": base_version.get("minor", 0),
        "patch": base_version.get("patch", 0),
        "status": base_version.get("status", ""),
        "build": base_version.get("build", ""),
        "module_config": base_version.get("module_config", ""),
        "docs_branch": base_version.get("docs_branch", "stable"),
        # Goblin-branded defaults
        "name": "Goblin Engine",
        "short_name": "Goblin",
        "website": "https://goblinengine.github.io/",
        "docs_url": "https://docs.goblinengine.org/en/",
        "community_name": "Goblin community",
        "contributors_name": "Goblin Engine contributors",
    }

    # Apply upstream overrides (base Version build data takes precedence over defaults).
    version_data.update({k: v for k, v in base_version.items() if v not in (None, "")})
    # Apply Goblin overrides from version_override.py.
    version_data.update({k: v for k, v in override_data.items() if v not in (None, "")})

    # Ensure docs_url is consistent with branch if only a base path was provided.
    if "docs_url" not in override_data:
        version_data["docs_url"] = version_data["docs_url"].rstrip("/") + "/"
    if version_data.get("docs_branch"):
        version_data["docs_url"] = f"{version_data['docs_url']}en/{version_data['docs_branch']}"

    # Final guardrails to avoid missing keys in format().
    required_defaults = {
        "status": version_data.get("status", ""),
        "build": version_data.get("build", ""),
        "module_config": version_data.get("module_config", ""),
        "docs_branch": version_data.get("docs_branch", "stable"),
    }
    version_data.update(required_defaults)
    
    with methods.generated_wrapper(str(target[0])) as file:
        file.write(
            """\
    #define GODOT_VERSION_SHORT_NAME "{short_name}"
    #define GODOT_VERSION_NAME "{name}"
    #define GODOT_VERSION_MAJOR {major}
    #define GODOT_VERSION_MINOR {minor}
    #define GODOT_VERSION_PATCH {patch}
    #define GODOT_VERSION_STATUS "{status}"
    #define GODOT_VERSION_BUILD "{build}"
    #define GODOT_VERSION_MODULE_CONFIG "{module_config}"
    #define GODOT_VERSION_WEBSITE "{website}"
    #define GODOT_VERSION_DOCS_BRANCH "{docs_branch}"
    #define GODOT_VERSION_DOCS_URL "{docs_url}"

    // Goblin Engine specific defines
    #define GOBLIN_BRANDING_ENABLED
    #define GOBLIN_ENGINE_NAME "{name}"
    #define GOBLIN_ENGINE_SHORT_NAME "{short_name}"
    #define GOBLIN_COMMUNITY_NAME "{community_name}"
    #define GOBLIN_CONTRIBUTORS_NAME "{contributors_name}"

    // String replacements for UI text (can be used in editor code)
    #ifndef GOBLIN_NO_STRING_OVERRIDE
    	// These help replace Godot strings in translatable strings
    	#define GODOT_COMMUNITY_OVERRIDE "{community_name}"
    	#define GODOT_ENGINE_CONTRIBUTORS_OVERRIDE "{contributors_name}"
    #endif
    """.format(**version_data)
        )


def goblin_authors_builder(target, source, env):
    """Generate authors with Goblin credits"""
    goblin_authors = os.path.join(os.path.dirname(__file__), "core", "AUTHORS.md")
    # Call ORIGINAL builder with goblin file
    return _original_core_builders['make_authors_header'](target, [env.File(goblin_authors)], env)


def goblin_donors_builder(target, source, env):
    """Generate donors with Goblin patrons"""
    goblin_donors = os.path.join(os.path.dirname(__file__), "core", "DONORS.md")
    # Call ORIGINAL builder with goblin file
    return _original_core_builders['make_donors_header'](target, [env.File(goblin_donors)], env)


def goblin_license_builder(target, source, env):
    """Generate license with Goblin copyright"""
    base_dir = os.path.dirname(__file__)
    goblin_copyright = os.path.join(base_dir, "core", "COPYRIGHT.txt")
    goblin_license = os.path.join(base_dir, "core", "LICENSE.txt")
    # Call ORIGINAL builder with goblin files
    return _original_core_builders['make_license_header'](target, [env.File(goblin_copyright), env.File(goblin_license)], env)


def goblin_splash_builder(target, source, env):
    """Generate splash screen with Goblin branding"""
    goblin_splash = os.path.join(os.path.dirname(__file__), "main", "splash.png")
    # Read Goblin splash, not source
    buffer = methods.get_buffer(goblin_splash)

    with methods.generated_wrapper(str(target[0])) as file:
        # Use a neutral gray color to better fit various kinds of projects.
        file.write(f"""\
static const Color boot_splash_bg_color = Color(0.14, 0.14, 0.14);
inline constexpr const unsigned char boot_splash_png[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")


def goblin_splash_editor_builder(target, source, env):
    """Generate editor splash screen with Goblin branding"""
    goblin_splash_editor = os.path.join(os.path.dirname(__file__), "main", "splash_editor.png")
    # Read Goblin splash, not source
    buffer = methods.get_buffer(goblin_splash_editor)

    with methods.generated_wrapper(str(target[0])) as file:
        # The editor splash background color is taken from the default editor theme's background color.
        # This helps achieve a visually "smoother" transition between the splash screen and the editor.
        file.write(f"""\
static const Color boot_splash_editor_bg_color = Color(0.125, 0.145, 0.192);
inline constexpr const unsigned char boot_splash_editor_png[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")


def goblin_app_icon_builder(target, source, env):
    """Generate app icon with Goblin branding"""
    goblin_app_icon = os.path.join(os.path.dirname(__file__), "main", "app_icon.png")
    # Read Goblin icon, not source
    buffer = methods.get_buffer(goblin_app_icon)

    with methods.generated_wrapper(str(target[0])) as file:
        # Use a neutral gray color to better fit various kinds of projects.
        file.write(f"""\
inline constexpr const unsigned char app_icon_png[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")


def goblin_ico_builder(target, source, env):
    """Generate goblin.ico (Windows exe icon) from app_icon.png.

    Writes a multi-resolution PNG-compressed ICO (Vista+ supports PNG
    entries) with standard Windows icon sizes: 256, 128, 64, 48, 32, 16.
    Falls back to a single 256x256 entry if Pillow is unavailable.
    """
    png_path = str(source[0])
    ico_path = str(target[0])

    try:
        from PIL import Image

        img = Image.open(png_path).convert("RGBA")
        sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
        img.save(ico_path, sizes=sizes)
    except ImportError:
        # Fallback: single 256x256 PNG-compressed ICO entry.
        with open(png_path, "rb") as f:
            png_data = f.read()

        # ICONDIR: reserved(2)=0, type(2)=1, count(2)=1
        header = struct.pack("<HHH", 0, 1, 1)
        # ICONDIRENTRY: width(1)=0 (means 256), height(1)=0, colors(1)=0,
        # reserved(1)=0, planes(2)=1, bitcount(2)=32, size(4), offset(4)=22
        entry = struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(png_data), 22)

        with open(ico_path, "wb") as f:
            f.write(header + entry + png_data)


def goblin_editor_icons_builder(target, source, env):
    """Override editor icons with Goblin branding
    
    This builder prioritizes Goblin icons over default Godot icons.
    Place your custom icons in modules/goblin/icons/ with the same names
    as the default Godot icons to override them.
    """
    # Filter sources to prioritize goblin icons
    goblin_icons = {}
    godot_icons = []
    
    for src in source:
        src_path = str(src)
        icon_name = os.path.basename(src_path)
        
        if "goblin/icons" in src_path:
            # Goblin icon takes precedence
            goblin_icons[icon_name] = src
        else:
            godot_icons.append(src)
    
    # Build final icon list: Goblin icons override, then remaining Godot icons
    final_sources = list(goblin_icons.values())
    
    for src in godot_icons:
        icon_name = os.path.basename(str(src))
        if icon_name not in goblin_icons:
            final_sources.append(src)
    
    # Use the editor icons builder with our custom source list
    return editor_icons_builders.make_editor_icons_action(target, final_sources, env)


def text_replacer_builder(target, source, env):
    """Post-process generated files to replace text references
    
    This can be used to patch generated C++ files to replace
    "Godot" with "Goblin" in string literals.
    """
    # Read the source file
    with open(str(source[0]), 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Apply replacements (be careful with these!)
    replacements = OrderedDict([
        ('"Godot Engine"', '"Goblin Engine"'),
        ('"Godot"', '"Goblin"'),
        ('"GODOT"', '"GOBLIN"'),
        ('"godot"', '"goblin"'),
        ('"Godot Engine contributors"', '"Goblin Engine contributors"'),
        ('"Godot community"', '"Goblin community"'),
        # Add more careful replacements as needed
        # Avoid replacing in code identifiers, only in strings
    ])
    
    for old, new in replacements.items():
        content = content.replace(old, new)
    
    # Write to target
    with open(str(target[0]), 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)
