# Goblin Branding - What Gets Renamed

## ✅ Automatically Renamed (via GODOT_VERSION_NAME define)

These use the `GODOT_VERSION_NAME` define which is set to "Goblin Engine":

1. **Window Titles**
   - Project Manager: "Goblin Engine - Project Manager"
   - Editor: "Goblin Engine - [Scene Name]"

2. **Console Output**
   - Startup banner: "Goblin Engine v4.x.x"
   - All version printing

3. **File Dialogs**
   - Import filters: "Goblin Engine Project"

4. **Resource Files (.rc)**
   - File description: "Goblin Engine"
   - Product name: "Goblin Engine"

5. **Generated Shader Comments**
   - "// NOTE: Shader automatically converted from Goblin Engine..."

6. **OpenGL Profile**
   - Application profile name: "Goblin Engine"

## ✅ Renamed via Generated Headers

These are replaced by the goblin module's generated files:

1. **AUTHORS.md** → `core/authors.gen.h`
   - Author credits and contributor lists

2. **DONORS.md** → `core/donors.gen.h`
   - Sponsor and patron lists

3. **COPYRIGHT.txt + LICENSE.txt** → `core/license.gen.h`
   - Copyright notices and license text

4. **Splash Screens**
   - Runtime splash: `main/splash.gen.h`
   - Editor splash: `main/splash_editor.gen.h`

5. **App Icon**
   - Window icon: `main/app_icon.gen.h`

6. **Editor Icons**
   - Logo in about screen (if you put Logo.svg in icons/ folder)
   - Any other SVG icons you add

## ⚠️ Partially Renamed / Manual Override Needed

These have hardcoded strings in C++ that need patches:

### About Dialog (editor/gui/editor_about.cpp)

**Strings that need patches:**
- Line 210: `"Thanks from the Godot community!"` 
  → Should be: `"Thanks from the Goblin community!"`
  
- Line 57: `"Godot Engine contributors"`
  → Should be: `"Goblin Engine contributors"`
  
- Line 305: `"Godot Engine relies on a number of third-party..."`
  → Should be: `"Goblin Engine relies on a number of third-party..."`

**How to fix these:**

You have two options:

### Option 1: Accept Some "Godot" References (Easiest)
Just note in your documentation that Goblin is based on Godot, and leave these strings as-is or add "(based on Godot)" notes.

### Option 2: Patch at Build Time (Moderate)
Add a post-processing step to replace strings in compiled .obj files before linking.

### Option 3: Override the About Dialog (Advanced)
Create a complete replacement EditorAbout class in the goblin module that inherits from AcceptDialog and replaces all strings.

## 📝 Recommended Approach

**For most users**: Use Option 1. The engine name, window titles, project manager, and all user-facing version strings already show "Goblin Engine". The few strings in the About dialog that still reference Godot can be seen as proper attribution to the base engine.

**Example About text:**
```
Thanks from the Goblin community!

© 2014-present Goblin Engine contributors.
© 2014-present Godot Engine contributors (Goblin is based on Godot).
© 2007-2014 Juan Linietsky, Ariel Manzur.

Goblin Engine is based on Godot Engine and relies on a number of 
third-party free and open source libraries...
```

This is honest, gives proper attribution, and doesn't require patching compiled code.

## Current Status Summary

✅ **Fully Working:**
- Executable name: `goblin.exe`
- Window title: "Goblin Engine"
- Version info everywhere: "Goblin Engine"
- Authors/Donors/License from your files
- Splash screens from your files
- App icon from your files

⚠️ **Partially Working:**
- About dialog logo: Works if you add `Logo.svg` to `modules/goblin/icons/`
- About dialog text: Uses custom AUTHORS.md, but a few hardcoded strings remain

❌ **Not Renamed (by design):**
- Source code comments (shouldn't be renamed)
- Internal class names (GODOT_ prefixes in C++)
- Build system internals

## Next Steps

1. Add `Logo.svg` to `modules/goblin/icons/` for the about screen logo
2. Decide if you want to patch the about dialog strings or acknowledge Godot as the base
3. Test the build and verify all visible strings show "Goblin"
