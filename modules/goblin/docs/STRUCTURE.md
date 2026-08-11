# Goblin Engine Module - Structure Overview

This module follows Godot's directory structure, making it easy to understand what parts of Godot are being patched/overridden.

## Directory Structure

```
modules/goblin/
├── config.py                    # Module configuration
├── register_types.cpp/h         # Module registration
├── SCsub                        # Main build script (orchestrates everything)
├── goblin_builders.py           # Build helper functions
│
├── core/                        # Patches for core/*
│   ├── SCsub
│   ├── AUTHORS.md              → Generates core/authors.gen.h
│   ├── DONORS.md               → Generates core/donors.gen.h  
│   ├── COPYRIGHT.txt           → Generates core/license.gen.h
│   ├── LICENSE.txt             → Generates core/license.gen.h
│   └── version_override.py     → Generates core/version_generated.gen.h
│
├── main/                        # Patches for main/*
│   ├── SCsub
│   ├── splash.png              → Generates main/splash.gen.h
│   ├── splash_editor.png       → Generates main/splash_editor.gen.h
│   └── app_icon.png            → Generates main/app_icon.gen.h
│
├── editor/                      # Patches for editor/*
│   ├── SCsub
│   ├── goblin_about.h          # Translation override system
│   ├── goblin_about.cpp
│   ├── icons/                  # Patches for editor/icons/*
│   │   ├── Logo.svg           → About screen logo
│   │   └── LogoOutlined.svg   → About screen logo outlined
│   └── themes/                 # Future: editor theme patches
│
├── doc_classes/                 # Module documentation classes
│
└── Documentation files:
    ├── README.md                # This file
    ├── BRANDING_STATUS.md       # What's branded and how
    ├── TRANSLATION_OVERRIDES.md # Translation system docs
    └── IMPLEMENTATION_STATUS.md # Current implementation status
```

## How It Works

### 1. Mirror Structure = Clear Patches

The structure mirrors Godot's main directories:
- `goblin/core/` → patches `godot/core/`
- `goblin/main/` → patches `godot/main/`
- `goblin/editor/` → patches `godot/editor/`

This makes it immediately clear **what you're patching** and **where it lives** in the main engine.

### 2. Self-Contained Build System

Everything is orchestrated from `goblin/SCsub`:
1. Generates core branding headers (version, authors, donors, license)
2. Calls `core/SCsub`, `main/SCsub`, `editor/SCsub` in sequence
3. Each subdirectory handles its own patches
4. No modification of Godot's core SCons files needed

### 3. Component Organization

**Core Branding** (`core/`)
- VERSION_NAME, authors, donors, copyright/license
- These are generated headers that replace Godot's equivalents

**Visual Branding** (`main/`)
- Splash screens, app icon
- Uses main_builders from Godot directly

**Editor Branding** (`editor/`)
- About dialog text via translation system
- Editor icons (Logo.svg, etc.)
- Future: editor themes, custom dialogs

### 4. Translation Override System

`editor/goblin_about.h` provides runtime string replacement:
- Hooks into TranslationServer at startup
- Replaces "Godot" → "Goblin" in UI strings
- No core code modifications needed

## Advantages of This Structure

1. **Visual Clarity**: You can see at a glance what's being patched
2. **Easy Maintenance**: Updates to Godot won't conflict with your changes
3. **Modular**: Each directory is self-contained with its own SCsub
4. **Scalable**: Easy to add more patches (scenes/, servers/, etc.)
5. **Clean**: No modifications to Godot's core build files

## Similar to Goost

This structure is inspired by [goost](https://github.com/goostengine/goost), which pioneered this pattern for Godot 3.x. Key similarities:
- Mirrored directory structure
- Self-contained build system
- Component-based organization
- No core modifications

## What Gets Patched

### Core Patches (`core/`)
- `AUTHORS.md` → `core/authors.gen.h`
- `DONORS.md` → `core/donors.gen.h`
- `COPYRIGHT.txt` + `LICENSE.txt` → `core/license.gen.h`
- `version_override.py` → `core/version_generated.gen.h`

### Main Patches (`main/`)
- `splash.png` → `main/splash.gen.h`
- `splash_editor.png` → `main/splash_editor.gen.h`
- `app_icon.png` → `main/app_icon.gen.h`

### Editor Patches (`editor/`)
- `icons/Logo.svg` → Included in `editor/icons/editor_icons.gen.h`
- `goblin_about.cpp` → Runtime translation overrides
- About dialog, Help menu, etc.

## Building

```bash
scons module_goblin_enabled=yes
```

Output:
- Executable: `bin/goblin.windows.editor.x86_64.exe`
- All branding applied automatically during build

## Adding New Patches

Want to patch something new?

1. Create the appropriate subdirectory (e.g., `goblin/scene/` to patch `godot/scene/`)
2. Add a `SCsub` file in that directory
3. Import `env_goblin` and add your patches
4. Update main `goblin/SCsub` to call your new SCsub

Example - adding scene patches:
```python
# goblin/scene/SCsub
Import("env_goblin")
env_goblin.add_source_files(env_goblin.goblin_sources, "*.cpp")
```

Then in `goblin/SCsub`:
```python
if os.path.exists(goblin_module_path + "/scene/SCsub"):
    SConscript("scene/SCsub")
```

## Future Possibilities

- `scene/` - Custom scene types, nodes
- `servers/` - Rendering server patches
- `modules/` - Sub-modules within Goblin!
- `tests/` - Goblin-specific tests
- `thirdparty/` - Goblin-specific dependencies

The sky's the limit! The structure supports it all.
