# Goblin Engine - About Screen Patches

This folder contains patches for editor UI strings that need to be customized.

## Files to create:

1. **editor_about_patch.cpp** - Replacement strings for the about dialog
2. **about_logo.png** - Optional: Custom about screen logo (otherwise uses Logo.svg from icons/)

## What gets patched:

- About dialog title: "Thanks from the Godot community!" → "Thanks from the Goblin community!"
- Copyright notice: "Godot Engine contributors" → "Goblin Engine contributors"
- Third-party notice: "Godot Engine relies on..." → "Goblin Engine relies on..."

## Note:

Most strings already use `GODOT_VERSION_NAME` define which is overridden by the version_override.py file.
Only a few hardcoded strings in the about dialog need explicit patching.
