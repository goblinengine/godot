#!/usr/bin/env python3
"""
Goblin Branding Manager
Utility script to help manage Goblin Engine branding files and test builds
"""

import os
import sys
import subprocess
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent.absolute()
GODOT_ROOT = SCRIPT_DIR.parent.parent
BRANDING_DIR = SCRIPT_DIR / "branding"
ICONS_DIR = SCRIPT_DIR / "icons"


def print_header(text):
    print("\n" + "=" * 60)
    print(f"  {text}")
    print("=" * 60)


def check_branding_files():
    """Verify all required branding files exist"""
    print_header("Checking Branding Files")
    
    required_files = [
        BRANDING_DIR / "AUTHORS.md",
        BRANDING_DIR / "DONORS.md",
        BRANDING_DIR / "COPYRIGHT.txt",
        BRANDING_DIR / "LICENSE.txt",
        BRANDING_DIR / "version_override.py",
    ]
    
    all_exist = True
    for file in required_files:
        if file.exists():
            print(f"✓ {file.name} exists")
        else:
            print(f"✗ {file.name} MISSING")
            all_exist = False
    
    return all_exist


def list_custom_icons():
    """List all custom Goblin icons"""
    print_header("Custom Goblin Icons")
    
    if not ICONS_DIR.exists():
        print("No custom icons directory found")
        return
    
    icons = list(ICONS_DIR.glob("*.svg"))
    if not icons:
        print("No custom icons found")
        print(f"Add SVG files to: {ICONS_DIR}")
        return
    
    print(f"Found {len(icons)} custom icon(s):")
    for icon in sorted(icons):
        print(f"  • {icon.name}")


def compare_with_godot_icons():
    """Show which Godot icons are being overridden"""
    print_header("Icon Override Status")
    
    godot_icons_dir = GODOT_ROOT / "editor" / "icons"
    
    if not ICONS_DIR.exists() or not godot_icons_dir.exists():
        print("Cannot compare - missing icon directories")
        return
    
    goblin_icons = {icon.name for icon in ICONS_DIR.glob("*.svg")}
    godot_icons = {icon.name for icon in godot_icons_dir.glob("*.svg")}
    
    overridden = goblin_icons & godot_icons
    new_icons = goblin_icons - godot_icons
    
    if overridden:
        print(f"\nOverriding {len(overridden)} Godot icon(s):")
        for icon in sorted(overridden):
            print(f"  ✓ {icon}")
    
    if new_icons:
        print(f"\nNew custom icons ({len(new_icons)}):")
        for icon in sorted(new_icons):
            print(f"  + {icon}")
    
    if not goblin_icons:
        print("No custom icons - Godot defaults will be used")


def test_build():
    """Run a test build to verify Goblin branding works"""
    print_header("Testing Goblin Build")
    
    os.chdir(GODOT_ROOT)
    
    print("Running: scons --version")
    subprocess.run(["scons", "--version"])
    
    print("\nRunning test build (this may take a while)...")
    print("Command: scons platform=linuxbsd target=editor --jobs=4")
    
    result = subprocess.run([
        "scons",
        "platform=linuxbsd",
        "target=editor",
        "--jobs=4"
    ])
    
    if result.returncode == 0:
        print("\n✓ Build successful!")
        print("Check bin/ directory for the Goblin Engine binary")
    else:
        print("\n✗ Build failed")
        print("Check error messages above")
    
    return result.returncode == 0


def create_branding_template():
    """Create example branding files if they don't exist"""
    print_header("Creating Branding Templates")
    
    templates = {
        "AUTHORS.md": """# Goblin Engine authors

Goblin Engine is a fork of Godot Engine with custom branding.

## Project Founders

    Your Name (username)

## Lead Developer

    Your Name (username)

## Developers

    Your Name (username)
""",
        "DONORS.md": """# Goblin Engine donors

## Patrons

    Patron Name 1

## Platinum sponsors

    Platinum Sponsor 1
""",
        "COPYRIGHT.txt": """Copyright (c) 2025-present Goblin Engine contributors
Copyright (c) 2014-present Godot Engine contributors

Permission is hereby granted, free of charge...
""",
        "LICENSE.txt": """MIT License

Copyright (c) 2025 Goblin Engine

Permission is hereby granted, free of charge...
"""
    }
    
    created = []
    for filename, content in templates.items():
        filepath = BRANDING_DIR / filename
        if not filepath.exists():
            filepath.write_text(content)
            created.append(filename)
            print(f"✓ Created {filename}")
        else:
            print(f"  {filename} already exists")
    
    if created:
        print(f"\nCreated {len(created)} template file(s)")
        print("Please edit these files with your branding information")
    else:
        print("\nAll branding files already exist")


def show_help():
    """Display help information"""
    print("""
Goblin Branding Manager

Usage: python goblin_manager.py [command]

Commands:
    check       Check if all required branding files exist
    icons       List custom Goblin icons
    compare     Compare Goblin icons with Godot defaults
    template    Create branding file templates
    build       Run a test build with Goblin branding
    all         Run check, icons, and compare
    help        Show this help message

Examples:
    python goblin_manager.py check
    python goblin_manager.py build
    python goblin_manager.py all
""")


def main():
    if len(sys.argv) < 2:
        show_help()
        return
    
    command = sys.argv[1].lower()
    
    if command == "check":
        check_branding_files()
    
    elif command == "icons":
        list_custom_icons()
    
    elif command == "compare":
        compare_with_godot_icons()
    
    elif command == "template":
        create_branding_template()
    
    elif command == "build":
        if check_branding_files():
            test_build()
        else:
            print("\n⚠ Missing required branding files!")
            print("Run: python goblin_manager.py template")
    
    elif command == "all":
        check_branding_files()
        list_custom_icons()
        compare_with_godot_icons()
    
    elif command == "help":
        show_help()
    
    else:
        print(f"Unknown command: {command}")
        show_help()


if __name__ == "__main__":
    main()
